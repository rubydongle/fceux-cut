/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/// \file
/// \brief Handles the graphical game display for the SDL implementation.

#include "sdl.h"
#include "sdl-opengl.h"
#include "../common/vidblit.h"
#include "../../fceu.h"
#include "../../version.h"
#include "../../video.h"

#include "../../utils/memory.h"

#include "sdl-icon.h"
#include "dface.h"

#include "../common/configSys.h"
#include "sdl-video.h"

#ifdef CREATE_AVI
#include "../videolog/nesvideos-piece.h"
#endif

#ifdef _GTK
#include "gui.h"
#include <gdk/gdkx.h>
#endif

#include <cstdio>
#include <cstring>
#include <cstdlib>

// GLOBALS
extern Config *g_config;

// STATIC GLOBALS
extern SDL_Surface *s_screen;

static SDL_Surface *s_BlitBuf; // Buffer when using hardware-accelerated blits.
static SDL_Surface *s_IconSurface = NULL;

static int s_curbpp;
static int s_srendline, s_erendline;
static int s_tlines;
static int s_inited;

#ifdef OPENGL
static int s_useOpenGL;
#endif
static double s_exs, s_eys;
static int s_eefx;
static int s_clipSides;
//static int s_fullscreen;
//static int noframe;
//static int s_nativeWidth = -1;
//static int s_nativeHeight = -1;

#define NWIDTH	(256 - (s_clipSides ? 16 : 0))
#define NOFFSET	(s_clipSides ? 8 : 0)

static int s_paletterefresh;

extern bool MaxSpeed;

/**
 * Attempts to destroy the graphical video display.  Returns 0 on
 * success, -1 on failure.
 */

//draw input aids if we are fullscreen
bool FCEUD_ShouldDrawInputAids()
{
	return false;//s_fullscreen!=0;
}
 
int
KillVideo()
{
	// if the IconSurface has been initialized, destroy it
//	if(s_IconSurface) {
//		SDL_FreeSurface(s_IconSurface);
//		s_IconSurface=0;
//	}

	// return failure if the video system was not initialized
	if(s_inited == 0)
		return -1;
    
	// if the rest of the system has been initialized, shut it down
#ifdef OPENGL
	// check for OpenGL and shut it down
	if(s_useOpenGL)
		KillOpenGL();
	else
#endif
		// shut down the system that converts from 8 to 16/32 bpp
		if(s_curbpp > 8)
			KillBlitToHigh();

	// shut down the SDL video sub-system
	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	s_inited = 0;
	return 0;
}


// this variable contains information about the special scaling filters
//static int s_sponge;

/**
 * These functions determine an appropriate scale factor for fullscreen/
 */
inline double GetXScale(int xres)
{
	return ((double)xres) / NWIDTH;
}
inline double GetYScale(int yres)
{
	return ((double)yres) / s_tlines;
}
void FCEUD_VideoChanged()
{
	int buf;
	g_config->getOption("SDL.PAL", &buf);
	if(buf)
		PAL = 1;
	else
		PAL = 0;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
int InitVideo(FCEUGI *gi)
{
	// This is a big TODO.  Stubbing this off into its own function,
	// as the SDL surface routines have changed drastically in SDL2
	// TODO - SDL2
}
#else
/**
 * Attempts to initialize the graphical video display.  Returns 0 on
 * success, -1 on failure.
 */
int
InitVideo(FCEUGI *gi)
{
	// XXX soules - const?  is this necessary?
	const SDL_VideoInfo *vinf;
	int error, flags = 0;
	//int doublebuf, xstretch, ystretch, xres, yres, show_fps;
	int show_fps;

	FCEUI_printf("%s->Initializing video...", __FUNCTION__);
	show_fps = 1;

	// check the starting, ending, and total scan lines
	FCEUI_GetCurrentVidSystem(&s_srendline, &s_erendline);
	s_tlines = s_erendline - s_srendline + 1;

	// initialize the SDL video subsystem if it is not already active
	if(!SDL_WasInit(SDL_INIT_VIDEO)) {
		error = SDL_InitSubSystem(SDL_INIT_VIDEO);
		if(error) {
			FCEUD_PrintError(SDL_GetError());
			return -1;
		}
	}
	s_inited = 1;

	// shows the cursor within the display window
//	SDL_ShowCursor(1);

	// determine if we can allocate the display on the video card
	vinf = SDL_GetVideoInfo();
	if(vinf->hw_available) {
		flags |= SDL_HWSURFACE;
	}
    

	// check to see if we are showing FPS
	FCEUI_SetShowFPS(show_fps);
    
	//if(noframe) {
	//	flags |= SDL_NOFRAME;
	//}

	// gives the SDL exclusive palette control... ensures the requested colors
	flags |= SDL_HWPALETTE;


	// 设置每bit位
	int desbpp = 32;
//	g_config->getOption("SDL.BitsPerPixel", &desbpp);

	s_exs = 4.0;
	s_eys = 4.0;
	s_eefx = 0;
	//g_config->getOption("SDL.XScale", &s_exs);
	//g_config->getOption("SDL.YScale", &s_eys);
	//g_config->getOption("SDL.SpecialFX", &s_eefx);
        
		// -Video Modes Tag-
//	if(s_sponge) {
//		if(s_sponge >= 4) {
//			s_exs = s_eys = 3;
//		} else {
//			s_exs = s_eys = 2;
//		}
//		s_eefx = 0;
//	}

	s_screen = SDL_SetVideoMode((int)(NWIDTH * s_exs),
						(int)(s_tlines * s_eys),
						desbpp, flags);
	if(!s_screen) {
		FCEUD_PrintError(SDL_GetError());
		return -1;
	}

	s_curbpp = s_screen->format->BitsPerPixel;
	if(!s_screen) {
		FCEUD_PrintError(SDL_GetError());
		KillVideo();
		return -1;
	}

	FCEU_printf(" Video Mode->: %d x %d x %d bpp %s\n",
				s_screen->w, s_screen->h, s_screen->format->BitsPerPixel,
				"");

	if(s_curbpp != 8 && s_curbpp != 16 && s_curbpp != 24 && s_curbpp != 32) {
		FCEU_printf("  Sorry, %dbpp modes are not supported by FCE Ultra.  Supported bit depths are 8bpp, 16bpp, and 32bpp.\n", s_curbpp);
		KillVideo();
		return -1;
	}

	s_paletterefresh = 1;

	// XXX soules - can't SDL do this for us?
	 // if using more than 8bpp, initialize the conversion routines
	if(s_curbpp > 8) {
	InitBlitToHigh(s_curbpp >> 3,
						s_screen->format->Rmask,
						s_screen->format->Gmask,
						s_screen->format->Bmask,
						s_eefx, 0, 0);//s_sponge, 0);
	}
	return 0;
}
#endif

/**
 * Toggles the full-screen display.
 */
void ToggleFS()
{
    // pause while we we are making the switch
	bool paused = FCEUI_EmulationPaused();
	if(!paused)
		FCEUI_ToggleEmulationPause();

	int error, fullscreen = 0;//s_fullscreen;

	// shut down the current video system
	KillVideo();

	// flip the fullscreen flag
	//g_config->setOption("SDL.Fullscreen", !fullscreen);
//#ifdef _GTK
//	if(noGui == 0)
//	{
//		if(!fullscreen)
//		showGui(0);
//		else
//			showGui(1);
//	}
//#endif
	// try to initialize the video
	error = InitVideo(GameInfo);
	if(error) {
		// if we fail, just continue with what worked before
//		g_config->setOption("SDL.Fullscreen", fullscreen);
		InitVideo(GameInfo);
	}
	// if we paused to make the switch; unpause
	if(!paused)
		FCEUI_ToggleEmulationPause();
}

static SDL_Color s_psdl[256];

/**
 * Sets the color for a particular index in the palette.
 */
void
FCEUD_SetPalette(uint8 index,
                 uint8 r,
                 uint8 g,
                 uint8 b)
{
	s_psdl[index].r = r;
	s_psdl[index].g = g;
	s_psdl[index].b = b;

	s_paletterefresh = 1;
}

/**
 * Gets the color for a particular index in the palette.
 */
void
FCEUD_GetPalette(uint8 index,
				uint8 *r,
				uint8 *g,
				uint8 *b)
{
	*r = s_psdl[index].r;
	*g = s_psdl[index].g;
	*b = s_psdl[index].b;
}

/** 
 * Pushes the palette structure into the underlying video subsystem.
 */
static void RedoPalette()
{
#ifdef OPENGL
	if(s_useOpenGL)
		SetOpenGLPalette((uint8*)s_psdl);
	else 
#endif
	{
		if(s_curbpp > 8) {
			SetPaletteBlitToHigh((uint8*)s_psdl);
		} else
		{
#if SDL_VERSION_ATLEAST(2, 0, 0)
			//TODO - SDL2
#else
			SDL_SetPalette(s_screen, SDL_PHYSPAL, s_psdl, 0, 256);
#endif
		}
	}
}
// XXX soules - console lock/unlock unimplemented?

///Currently unimplemented.
void LockConsole(){}

///Currently unimplemented.
void UnlockConsole(){}

/**
 * Pushes the given buffer of bits to the screen.
 */
void
BlitScreen(uint8 *XBuf)
{
	SDL_Surface *TmpScreen;
	uint8 *dest;
	int xo = 0, yo = 0;

	if(!s_screen) {
		return;
	}

	// refresh the palette if required
	if(s_paletterefresh) {
		RedoPalette();
		s_paletterefresh = 0;
	}

	// XXX soules - not entirely sure why this is being done yet
	XBuf += s_srendline * 256;

	if(s_BlitBuf) {
		TmpScreen = s_BlitBuf;
	} else {
		TmpScreen = s_screen;
	}

	// lock the display, if necessary
	if(SDL_MUSTLOCK(TmpScreen)) {
		if(SDL_LockSurface(TmpScreen) < 0) {
			return;
		}
	}

	dest = (uint8*)TmpScreen->pixels;

	// XXX soules - again, I'm surprised SDL can't handle this
	// perform the blit, converting bpp if necessary
	if(s_curbpp > 8) {
		if(s_BlitBuf) {
			Blit8ToHigh(XBuf + NOFFSET, dest, NWIDTH, s_tlines,
						TmpScreen->pitch, 1, 1);
		} else {
			Blit8ToHigh(XBuf + NOFFSET, dest, NWIDTH, s_tlines,
						TmpScreen->pitch, (int)s_exs, (int)s_eys);
		}
	} else {
		if(s_BlitBuf) {
			Blit8To8(XBuf + NOFFSET, dest, NWIDTH, s_tlines,
					TmpScreen->pitch, 1, 1, 0, 0);//s_sponge);
		} else {
			Blit8To8(XBuf + NOFFSET, dest, NWIDTH, s_tlines,
					TmpScreen->pitch, (int)s_exs, (int)s_eys,
					s_eefx, 0);//s_sponge);
		}
	}

	// unlock the display, if necessary
	if(SDL_MUSTLOCK(TmpScreen)) {
		SDL_UnlockSurface(TmpScreen);
	}

	 // if we have a hardware video buffer, do a fast video->video copy
	if(s_BlitBuf) {
		SDL_Rect srect;
		SDL_Rect drect;

		srect.x = 0;
		srect.y = 0;
		srect.w = NWIDTH;
		srect.h = s_tlines;

		drect.x = 0;
		drect.y = 0;
		drect.w = (Uint16)(s_exs * NWIDTH);
		drect.h = (Uint16)(s_eys * s_tlines);

		SDL_BlitSurface(s_BlitBuf, &srect, s_screen, &drect);
	}

	 // ensure that the display is updated
#if SDL_VERSION_ATLEAST(2, 0, 0)
	//TODO - SDL2
#else
	SDL_UpdateRect(s_screen, xo, yo,
				(Uint32)(NWIDTH * s_exs), (Uint32)(s_tlines * s_eys));
#endif


#if SDL_VERSION_ATLEAST(2, 0, 0)
	// TODO
#else
    // have to flip the displayed buffer in the case of double buffering
	if(s_screen->flags & SDL_DOUBLEBUF) {
		SDL_Flip(s_screen);
	}
#endif
}

/**
 *  Converts an x-y coordinate in the window manager into an x-y
 *  coordinate on FCEU's screen.
 */
uint32
PtoV(uint16 x,
	uint16 y)
{
	y = (uint16)((double)y / s_eys);
	x = (uint16)((double)x / s_exs);
	if(s_clipSides) {
		x += 8;
	}
	y += s_srendline;
	return (x | (y << 16));
}

bool enableHUDrecording = false;
bool FCEUI_AviEnableHUDrecording()
{
	if (enableHUDrecording)
		return true;

	return false;
}
void FCEUI_SetAviEnableHUDrecording(bool enable)
{
	enableHUDrecording = enable;
}

bool disableMovieMessages = false;
bool FCEUI_AviDisableMovieMessages()
{
	if (disableMovieMessages)
		return true;

	return false;
}
void FCEUI_SetAviDisableMovieMessages(bool disable)
{
	disableMovieMessages = disable;
}

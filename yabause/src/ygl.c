/*  Copyright 2005 Guillaume Duhamel
    Copyright 2005 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include "ygl.h"

YglTextureManager * YglTM;
Ygl * _Ygl;

typedef struct
{
   u32 id;
   int * textdata;
} cache_struct;

static cache_struct *cachelist;
static int cachelistsize=0;

//////////////////////////////////////////////////////////////////////////////

void YglTMInit(unsigned int w, unsigned int h) {
   YglTM = (YglTextureManager *) malloc(sizeof(YglTextureManager));
   YglTM->texture = (unsigned int *) malloc(sizeof(unsigned int) * w * h);
   YglTM->width = w;
   YglTM->height = h;

   YglTMReset();
}

//////////////////////////////////////////////////////////////////////////////

void YglTMDeInit(void) {
   free(YglTM->texture);
   free(YglTM);
}

//////////////////////////////////////////////////////////////////////////////

void YglTMReset(void) {
   YglTM->currentX = 0;
   YglTM->currentY = 0;
   YglTM->yMax = 0;
}

//////////////////////////////////////////////////////////////////////////////

void YglTMAllocate(YglTexture * output, unsigned int w, unsigned int h, unsigned int * x, unsigned int * y) {
   if ((YglTM->height - YglTM->currentY) < h) {
      fprintf(stderr, "can't allocate texture: %dx%d\n", w, h);
      *x = *y = 0;
      output->w = 0;
      output->textdata = YglTM->texture;
      return;
   }

   if ((YglTM->width - YglTM->currentX) >= w) {
      *x = YglTM->currentX;
      *y = YglTM->currentY;
      output->w = YglTM->width - w;
      output->textdata = YglTM->texture + YglTM->currentY * YglTM->width + YglTM->currentX;
      YglTM->currentX += w;
      if ((YglTM->currentY + h) > YglTM->yMax)
         YglTM->yMax = YglTM->currentY + h;
   }
   else {
      YglTM->currentX = 0;
      YglTM->currentY = YglTM->yMax;
      YglTMAllocate(output, w, h, x, y);
   }
}

//////////////////////////////////////////////////////////////////////////////

int YglGLInit(int width, int height) {
   glClear(GL_COLOR_BUFFER_BIT);

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(0, 320, 224, 0, 1, 0);

   glMatrixMode(GL_TEXTURE);
   glLoadIdentity();
   glOrtho(-width, width, -height, height, 1, 0);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glGenTextures(1, &_Ygl->texture);
   glBindTexture(GL_TEXTURE_2D, _Ygl->texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, YglTM->texture);
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
}

//////////////////////////////////////////////////////////////////////////////

int YglInit(int width, int height, unsigned int depth) {
   unsigned int i;
   char yab_version[64];

   YglTMInit(width, height);

   if ((_Ygl = (Ygl *) malloc(sizeof(Ygl))) == NULL)
      return -1;
   _Ygl->depth = depth;
   if ((_Ygl->levels = (YglLevel *) malloc(sizeof(YglLevel) * depth)) == NULL)
      return -1;
   for(i = 0;i < depth;i++) {
      _Ygl->levels[i].currentQuad = 0;
      _Ygl->levels[i].maxQuad = 8 * 2000;
      if ((_Ygl->levels[i].quads = (int *) malloc(_Ygl->levels[i].maxQuad * sizeof(int))) == NULL)
         return -1;

      if ((_Ygl->levels[i].textcoords = (int *) malloc(_Ygl->levels[i].maxQuad * sizeof(int))) == NULL)
         return -1;
   }

   SDL_InitSubSystem( SDL_INIT_VIDEO );

   //set the window title
   sprintf(yab_version, "Yabause " VERSION);
   SDL_WM_SetCaption(yab_version, NULL);
	
   SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 4 );
   SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 4 );
   SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 4 );
   SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 4);
   SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
   SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

   if ( SDL_SetVideoMode( 320, 224, 32, SDL_OPENGL | SDL_RESIZABLE ) == NULL ) {
      fprintf(stderr, "Couldn't set GL mode: %s\n", SDL_GetError());
      SDL_Quit();
      return -1;
   }

   YglGLInit(width, height);

   _Ygl->st = 0;
   _Ygl->msglength = 0;

   // This is probably wrong, but it'll have to do for now
   if ((cachelist = (cache_struct *)malloc(0x100000 / 8 * sizeof(cache_struct))) == NULL)
      return -1;

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void YglDeInit(void) {
   int i;

   YglTMDeInit();

   if (_Ygl)
   {                   
      if (_Ygl->levels)
      {
         for (i = 0; i < _Ygl->depth; i++)
         {
            if (_Ygl->levels[i].quads)
               free(_Ygl->levels[i].quads);
            if (_Ygl->levels[i].textcoords)
               free(_Ygl->levels[i].textcoords);
         }
         free(_Ygl->levels);
      }

      free(_Ygl);
   }

   if (cachelist)
      free(cachelist);
}

//////////////////////////////////////////////////////////////////////////////

int * YglQuad(YglSprite * input, YglTexture * output) {
   unsigned int x, y;
   YglLevel *level;
   int *tmp;

   level = &_Ygl->levels[input->priority];

   if (level->currentQuad == level->maxQuad) {
      level->maxQuad += 8;
      level->quads = (int *) realloc(level->quads, level->maxQuad * sizeof(int));
      level->textcoords = (int *) realloc(level->textcoords, level->maxQuad * sizeof(int));
      YglCacheReset();
   }

   tmp = level->textcoords + level->currentQuad;

   memcpy(level->quads + level->currentQuad, input->vertices, 8 * sizeof(int));
   level->currentQuad += 8;
   YglTMAllocate(output, input->w, input->h, &x, &y);

   if (input->flip & 0x1) {
      *tmp = *(tmp + 6) = x + input->w;
      *(tmp + 2) = *(tmp + 4) = x;
   } else {
      *tmp = *(tmp + 6) = x;
      *(tmp + 2) = *(tmp + 4) = x + input->w;
   }

   if (input->flip & 0x2) {
      *(tmp + 1) = *(tmp + 3) = y + input->h;
      *(tmp + 5) = *(tmp + 7) = y;
   } else {
      *(tmp + 1) = *(tmp + 3) = y;
      *(tmp + 5) = *(tmp + 7) = y + input->h;
   }

   switch(input->flip) {
      case 0:
         return level->textcoords + level->currentQuad - 8;
      case 1:
         return level->textcoords + level->currentQuad - 6;
      case 2:
         return level->textcoords + level->currentQuad - 2;
      case 3:
         return level->textcoords + level->currentQuad - 4;
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void YglCachedQuad(YglSprite * input, int * cache) {
   YglLevel * level = _Ygl->levels + input->priority;
   unsigned int x,y;
   int * tmp;

   x = *cache;
   y = *(cache + 1);

   if (level->currentQuad == level->maxQuad) {
      level->maxQuad += 8;
      level->quads = (int *) realloc(level->quads, level->maxQuad * sizeof(int));
      level->textcoords = (int *) realloc(level->textcoords, level->maxQuad * sizeof(int));
      YglCacheReset();
   }

   tmp = level->textcoords + level->currentQuad;

   memcpy(level->quads + level->currentQuad, input->vertices, 8 * sizeof(int));
   level->currentQuad += 8;

   if (input->flip & 0x1) {
      *tmp = *(tmp + 6) = x + input->w;
      *(tmp + 2) = *(tmp + 4) = x;
   } else {
      *tmp = *(tmp + 6) = x;
      *(tmp + 2) = *(tmp + 4) = x + input->w;
   }
   if (input->flip & 0x2) {
      *(tmp + 1) = *(tmp + 3) = y + input->h;
      *(tmp + 5) = *(tmp + 7) = y;
   } else {
      *(tmp + 1) = *(tmp + 3) = y;
      *(tmp + 5) = *(tmp + 7) = y + input->h;
   }
}

//////////////////////////////////////////////////////////////////////////////

void YglRender(void) {
   YglLevel * level;
   int i;

   glEnable(GL_TEXTURE_2D);

   glBindTexture(GL_TEXTURE_2D, _Ygl->texture);

   glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, YglTM->width, YglTM->yMax, GL_RGBA, GL_UNSIGNED_BYTE, YglTM->texture);

   if(_Ygl->st) {
      int vertices [] = { 0, 0, 320, 0, 320, 224, 0, 224 };
      int text [] = { 0, 0, YglTM->width, 0, YglTM->width, YglTM->height, 0, YglTM->height };
      glVertexPointer(2, GL_INT, 0, vertices);
      glTexCoordPointer(2, GL_INT, 0, text);
      glDrawArrays(GL_QUADS, 0, 4);
   } else {
      for(i = 0;i < _Ygl->depth;i++) {
         level = _Ygl->levels + i;
         glVertexPointer(2, GL_INT, 0, level->quads);
         glTexCoordPointer(2, GL_INT, 0, level->textcoords);
         glDrawArrays(GL_QUADS, 0, level->currentQuad / 2);
      }
   }

   glDisable(GL_TEXTURE_2D);
#ifndef _arch_dreamcast
#if HAVE_LIBGLUT
   if (_Ygl->msglength > 0) {
      glColor3f(1.0f, 0.0f, 0.0f);
      glRasterPos2i(10, 22);
      for (i = 0; i < _Ygl->msglength; i++) {
         glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, _Ygl->message[i]);
      }
      glColor3f(1, 1, 1);
   }
#endif
#endif

   SDL_GL_SwapBuffers();
}

//////////////////////////////////////////////////////////////////////////////

void YglReset(void) {
   YglLevel * level;
   int i;

   glClear(GL_COLOR_BUFFER_BIT);

   YglTMReset();

   for(i = 0;i < _Ygl->depth;i++) {
      level = _Ygl->levels + i;
      level->currentQuad = 0;
   }
   _Ygl->msglength = 0;
}

//////////////////////////////////////////////////////////////////////////////

void YglShowTexture(void) {
   _Ygl->st = !_Ygl->st;
}

//////////////////////////////////////////////////////////////////////////////

void YglChangeResolution(int w, int h) {
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(0, w, h, 0, 1, 0);
}

//////////////////////////////////////////////////////////////////////////////

void YglOnScreenDebugMessage(char *string, ...) {
   va_list arglist;

   va_start(arglist, string);
   vsprintf(_Ygl->message, string, arglist);
   va_end(arglist);
   _Ygl->msglength = strlen(_Ygl->message);
}

//////////////////////////////////////////////////////////////////////////////

int * YglIsCached(u32 addr) {
   int i = 0;

   for (i = 0; i < cachelistsize; i++)
   {
      if (addr == cachelist[i].id)
         return cachelist[i].textdata;
   }

   return NULL;
}

//////////////////////////////////////////////////////////////////////////////

void YglCache(u32 addr, int * val) {
   cachelist[cachelistsize].id = addr;
   cachelist[cachelistsize].textdata = val;
   cachelistsize++;
}

//////////////////////////////////////////////////////////////////////////////

void YglCacheReset(void) {
   cachelistsize = 0;
}

//////////////////////////////////////////////////////////////////////////////

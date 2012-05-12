// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tool is used to benchmark the technique of using tiles to render a
// large tecture. This tool runs with different tile sizes so that developer
// can determine the right dimensions for the tiles.

#include <iostream>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

// Initial size of the window and the texture.
static const int kStartSize = 1024;

// The smallest texture size we're trying.
static const int kEndSize = 16;

// Factor for dividing the size from one step to another.
static const int kSizeFactor = 2;

// Number of renders before we output.
static const int kRenderCount = 1000;
static const int kMaxTextures = kStartSize * kStartSize / kEndSize / kEndSize;

Display* g_display = NULL;
Window g_window = 0;
bool g_running = false;
GLXContext g_gl_context = NULL;
GLuint g_textures[kMaxTextures];
scoped_array<uint8> g_image;

// Initialize X11. Returns true if successful. This method creates the X11
// window. Further initialization is done in X11VideoRenderer.
bool InitX11() {
  g_display = XOpenDisplay(NULL);
  if (!g_display) {
    std::cout << "Error - cannot open display" << std::endl;
    return false;
  }

  // Get properties of the screen.
  int screen = DefaultScreen(g_display);
  int root_window = RootWindow(g_display, screen);

  // Creates the window.
  g_window = XCreateSimpleWindow(g_display, root_window, 1, 1, 100, 50, 0,
                                 BlackPixel(g_display, screen),
                                 BlackPixel(g_display, screen));
  XStoreName(g_display, g_window, "Tile Render Bench");

  XSelectInput(g_display, g_window,
               ExposureMask | ButtonPressMask | KeyPressMask);
  XMapWindow(g_display, g_window);

  XResizeWindow(g_display, g_window, kStartSize, kStartSize);

  return true;
}

// Initialize the OpenGL context.
void InitGLContext() {
  if (!InitializeGLBindings(gfx::kGLImplementationDesktopGL)) {
    LOG(ERROR) << "InitializeGLBindings failed";
    return;
  }

  XWindowAttributes attributes;
  XGetWindowAttributes(g_display, g_window, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);
  int visual_info_count = 0;
  XVisualInfo* visual_info_list = XGetVisualInfo(g_display, VisualIDMask,
                                                 &visual_info_template,
                                                 &visual_info_count);

  for (int i = 0; i < visual_info_count && !g_gl_context; ++i) {
    g_gl_context = glXCreateContext(g_display, visual_info_list + i, 0,
                                    True /* Direct rendering */);
  }

  XFree(visual_info_list);
  if (!g_gl_context) {
    return;
  }

  if (!glXMakeCurrent(g_display, g_window, g_gl_context)) {
    glXDestroyContext(g_display, g_gl_context);
    g_gl_context = NULL;
    return;
  }
}

void InitTest() {
  g_image.reset(new uint8[kStartSize * kStartSize]);
  for (int i = 0; i < kStartSize; ++i) {
    for (int j = 0; j < kStartSize; ++j) {
      bool white = (i / 16) % 2 == (j / 16) % 2;
      g_image.get()[i * kStartSize + j] = white ? 255 : 0;
    }
  }

  glGenTextures(kMaxTextures, g_textures);
  for (int i = 0; i < kMaxTextures; ++i) {
    glBindTexture(GL_TEXTURE_2D, g_textures[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  glEnable(GL_TEXTURE_2D);
}

void UpdateTextures(int tile_size) {
  int k = 0;
  uint8* image = g_image.get();
  for (int y = 0; y < kStartSize; y += tile_size) {
    for (int x = 0; x < kStartSize; x += tile_size) {
      glBindTexture(GL_TEXTURE_2D, g_textures[k]);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, kStartSize);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tile_size, tile_size, 0,
                   GL_LUMINANCE, GL_UNSIGNED_BYTE, image);
      ++k;
      image += tile_size;
    }
    image += tile_size * kStartSize - kStartSize;
  }
}

void Render(int tile_size) {
  float points[] ={
    0, 0, 0,
    1, 0, 0,
    1, 1, 0,
    0, 1, 0
  };
  GLfloat tex_coords[] = {0, 0, 1, 0, 1, 1, 0, 1};

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  glVertexPointer(3, GL_FLOAT, 0, points);
  glTexCoordPointer(2, GL_FLOAT, 0, tex_coords);

  // This will tile them from bottom-up. I'm too lazy to think about top-down.
  int k = 0;
  int scale_factor = kStartSize / tile_size;
  double step = 1 / scale_factor;
  for (int y = 0; y < kStartSize; y += tile_size) {
    for (int x = 0; x < kStartSize; x += tile_size) {
      glBindTexture(GL_TEXTURE_2D, g_textures[k++]);
      glPushMatrix();
      glTranslated(-1, -1, 0);
      glScaled(2.0 / scale_factor, 2.0 / scale_factor, 0);
      glTranslated(x / tile_size * step, y / tile_size * step, 0);
      glDrawArrays(GL_QUADS, 0, 4);
      glPopMatrix();
    }
  }

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  glXSwapBuffers(g_display, g_window);
}

void RunTest() {
  static int tile_size = kStartSize;
  static int count = kRenderCount;
  static int64 start_time = 0;

  // If this is 0 that means this is the first time we render for this size.
  if (!start_time) {
    start_time = base::Time::Now().ToInternalValue();
  }

  UpdateTextures(tile_size);
  Render(tile_size);

  // We have finished rendering for this size, output the numbers.
  if (--count == 0) {
    base::TimeDelta delta = base::Time::Now() -
        base::Time::FromInternalValue(start_time);
    std::cout << "size =  " << tile_size
              << " speed = " << delta.InMilliseconds()
              << std::endl;
    std::cout.flush();
    tile_size /= kSizeFactor;
    count = kRenderCount;
    start_time = 0;
  }

  if (tile_size <= kEndSize) {
    MessageLoop::current()->Quit();
    return;
  }

  XExposeEvent ev = { Expose, 0, 1, g_display, g_window,
                      0, 0, kStartSize, kStartSize, 0 };
  XSendEvent(g_display, g_window, False, ExposureMask, (XEvent*)&ev);

  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&RunTest));
}

void ProcessEvents() {
  // Consume all the X events
  while (XPending(g_display)) {
    XEvent e;
    XNextEvent(g_display, &e);
    switch (e.type) {
      case Expose:
        RunTest();
      default:
        break;
    }
  }
}

int main() {
  base::AtExitManager at_exit;
  MessageLoop loop;
  InitX11();
  InitGLContext();
  InitTest();

  loop.PostTask(FROM_HERE, base::Bind(&ProcessEvents));
  loop.Run();

  // Cleanup GL.
  glXMakeCurrent(g_display, 0, NULL);
  glXDestroyContext(g_display, g_gl_context);

  // Destroy window and display.
  XDestroyWindow(g_display, g_window);
  XCloseDisplay(g_display);
  return 0;
}

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

extern "C" {

#if defined(GLES2_CONFORM_SUPPORT_ONLY)
#include "gpu/gles2_conform_support/gtf/gtf_stubs.h"
#else
#include "third_party/gles2_conform/GTF_ES/glsl/GTF/Source/eglNative.h"
#endif

GTFbool GTFNativeCreateDisplay(EGLNativeDisplayType *pNativeDisplay) {
  int argc = 0;
  char **argv = NULL;
  gtk_init(&argc, &argv);
  *pNativeDisplay = GDK_DISPLAY();;
  return *pNativeDisplay ? GTFtrue : GTFfalse;
}

void GTFNativeDestroyDisplay(EGLNativeDisplayType nativeDisplay) {
  gtk_exit(0);
}

void GTFNativeDestroyWindow(EGLNativeDisplayType nativeDisplay,
                            EGLNativeWindowType nativeWindow) {
  GdkWindow* window = gdk_window_lookup(nativeWindow);
  gpointer widget = NULL;
  gdk_window_get_user_data(window, &widget);
  gtk_widget_destroy(GTK_WIDGET(widget));
}

GTFbool GTFNativeCreateWindow(EGLNativeDisplayType nativeDisplay,
                              EGLDisplay eglDisplay, EGLConfig eglConfig,
                              const char* title, int width, int height,
                              EGLNativeWindowType *pNativeWindow) {
#ifdef CHROMEOS_GLES2_CONFORMANCE
  // Due to the behavior of ChromeOS window manager, which always resize the
  // top level window etc, we had to create a popup window.
  GtkWidget* hwnd = gtk_window_new(GTK_WINDOW_POPUP);
#else
  GtkWidget* hwnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#endif

  gtk_window_set_title(GTK_WINDOW(hwnd), title);
  gtk_window_set_default_size(GTK_WINDOW(hwnd), width, height);
  gtk_widget_set_double_buffered(hwnd, FALSE);
  gtk_widget_set_app_paintable(hwnd, TRUE);

  // We had to enter gtk main loop to realize the window on ChromeOS.
  gtk_widget_show_now(hwnd);

  *pNativeWindow = GDK_WINDOW_XWINDOW(hwnd->window);

  return GTFtrue;
}

}  // extern "C"

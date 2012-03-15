// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/window.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"

namespace {
using gpu::demos::Window;

gboolean OnDelete(GtkWidget* widget, GdkEventExpose* event) {
  gtk_main_quit();
  return FALSE;
}

gboolean OnExpose(GtkWidget* widget, GdkEventExpose* event, gpointer data) {
  Window* window = static_cast<Window*>(data);
  window->OnPaint();

  gtk_widget_queue_draw(widget);
  return FALSE;
}
}  // namespace

namespace gpu {
namespace demos {

void Window::MainLoop() {
  gtk_signal_connect(GTK_OBJECT(window_handle_),
                     "delete_event",
                     reinterpret_cast<GtkSignalFunc>(OnDelete),
                     NULL);
  gtk_signal_connect(GTK_OBJECT(window_handle_),
                     "expose_event",
                     reinterpret_cast<GtkSignalFunc>(OnExpose),
                     this);
  gtk_main();
}

gfx::NativeWindow Window::CreateNativeWindow(const wchar_t* title,
                                             int width, int height) {
  GtkWidget* hwnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title(GTK_WINDOW(hwnd), WideToUTF8(title).c_str());
  gtk_window_set_default_size(GTK_WINDOW(hwnd), width, height);
  gtk_widget_set_double_buffered(hwnd, FALSE);
  gtk_widget_set_app_paintable(hwnd, TRUE);

  gtk_widget_show(hwnd);
  return GTK_WINDOW(hwnd);
}

gfx::AcceleratedWidget Window::PluginWindow(gfx::NativeWindow hwnd) {
  return GDK_WINDOW_XWINDOW(GTK_WIDGET(hwnd)->window);
}

}  // namespace demos
}  // namespace gpu


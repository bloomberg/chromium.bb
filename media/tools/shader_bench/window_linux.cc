// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/shader_bench/window.h"

#include "base/task.h"
#include "media/tools/shader_bench/painter.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

namespace media {

static gboolean OnDelete(GtkWidget* widget, GdkEventExpose* event) {
  gtk_main_quit();
  return FALSE;
}

static gboolean OnExpose(GtkWidget* widget,
                         GdkEventExpose* event,
                         gpointer data) {
  Window* window = reinterpret_cast<Window*>(data);
  if (window)
    window->OnPaint();
  return FALSE;
}

gfx::NativeWindow Window::CreateNativeWindow(int width, int height) {
  GtkWidget* hwnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_default_size(GTK_WINDOW(hwnd), width, height);
  gtk_widget_set_double_buffered(hwnd, FALSE);
  gtk_widget_set_app_paintable(hwnd, TRUE);
  gtk_widget_show(hwnd);

  return GTK_WINDOW(hwnd);
}

gfx::PluginWindowHandle Window::PluginWindow() {
  return GDK_WINDOW_XWINDOW(GTK_WIDGET(window_handle_)->window);
}

void Window::Start(int limit, Task* done_task, Painter* painter) {
  running_ = true;
  count_ = 0;
  limit_ = limit;
  done_task_ = done_task;
  painter_ = painter;

  gtk_signal_connect(GTK_OBJECT(window_handle_),
                     "delete_event",
                     reinterpret_cast<GtkSignalFunc>(OnDelete),
                     NULL);

  gtk_signal_connect(GTK_OBJECT(window_handle_),
                     "expose_event",
                     reinterpret_cast<GtkSignalFunc>(OnExpose),
                     this);

  gtk_widget_queue_draw(GTK_WIDGET(window_handle_));
  MainLoop();
}

void Window::OnPaint() {
  if (!running_)
    return;

  if (count_ < limit_) {
    painter_->OnPaint();
    count_++;
    gtk_widget_queue_draw(GTK_WIDGET(window_handle_));
  } else {
    running_ = false;
    if (done_task_) {
      done_task_->Run();
      delete done_task_;
    }
    gtk_main_quit();
  }
}

void Window::MainLoop() {
  gtk_main();
}

}  // namespace media

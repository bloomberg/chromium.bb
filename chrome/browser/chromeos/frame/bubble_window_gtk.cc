// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/bubble_window.h"

#include <gtk/gtk.h>

#include "chrome/browser/chromeos/frame/bubble_frame_view.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "ui/views/window/non_client_view.h"

namespace chromeos {

BubbleWindow::BubbleWindow(views::Widget* window,
    DialogStyle style)
    : views::NativeWidgetGtk(window),
      style_(style) {
}

void BubbleWindow::InitNativeWidget(const views::Widget::InitParams& params) {
  views::NativeWidgetGtk::InitNativeWidget(params);

  // Turn on double buffering so that the hosted GtkWidgets does not
  // flash as in http://crosbug.com/9065.
  EnableDoubleBuffer(true);

  GdkColor background_color =
      gfx::SkColorToGdkColor(kBubbleWindowBackgroundColor);
  gtk_widget_modify_bg(GetNativeView(), GTK_STATE_NORMAL, &background_color);

  // A work-around for http://crosbug.com/8538. All GdkWindow of top-level
  // GtkWindow should participate _NET_WM_SYNC_REQUEST protocol and window
  // manager should only show the window after getting notified. And we
  // should only notify window manager after at least one paint is done.
  // TODO(xiyuan): Figure out the right fix.
  gtk_widget_realize(GetNativeView());
  gdk_window_set_back_pixmap(GetNativeView()->window, NULL, FALSE);
  gtk_widget_realize(window_contents());
  gdk_window_set_back_pixmap(window_contents()->window, NULL, FALSE);
}

views::NonClientFrameView* BubbleWindow::CreateNonClientFrameView() {
  views::Widget* window = GetWidget();
  return new BubbleFrameView(window, window->widget_delegate(), style_);
}

views::Widget* BubbleWindow::Create(
    gfx::NativeWindow parent,
    DialogStyle style,
    views::WidgetDelegate* widget_delegate) {
  views::Widget* window = new views::Widget;
  BubbleWindow* bubble_window = new BubbleWindow(window, style);
  views::Widget::InitParams params;
  params.delegate = widget_delegate;
  params.native_widget = bubble_window;
  params.parent = GTK_WIDGET(parent);
  params.bounds = gfx::Rect();
  window->Init(params);

  return window;
}

}  // namespace chromeos

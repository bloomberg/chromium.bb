// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/bubble_window.h"

#include <gtk/gtk.h>

#include "chrome/browser/chromeos/frame/bubble_frame_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "cros/chromeos_wm_ipc_enums.h"
#include "gfx/skia_utils_gtk.h"
#include "views/window/non_client_view.h"

namespace chromeos {

// static
const SkColor BubbleWindow::kBackgroundColor = SK_ColorWHITE;

BubbleWindow::BubbleWindow(views::WindowDelegate* window_delegate)
    : views::WindowGtk(window_delegate) {
  MakeTransparent();
}

void BubbleWindow::Init(GtkWindow* parent, const gfx::Rect& bounds) {
  views::WindowGtk::Init(parent, bounds);

  GdkColor background_color = gfx::SkColorToGdkColor(kBackgroundColor);
  gtk_widget_modify_bg(GetNativeView(), GTK_STATE_NORMAL, &background_color);
}

views::Window* BubbleWindow::Create(
    gfx::NativeWindow parent,
    const gfx::Rect& bounds,
    views::WindowDelegate* window_delegate) {
  BubbleWindow* window = new BubbleWindow(window_delegate);
  window->GetNonClientView()->SetFrameView(new BubbleFrameView(window));
  window->Init(parent, bounds);

  chromeos::WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
      NULL);

  return window;
}

}  // namespace chromeos

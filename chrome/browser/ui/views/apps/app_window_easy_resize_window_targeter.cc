// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_window_easy_resize_window_targeter.h"

#include "ui/aura/window.h"
#include "ui/base/base_window.h"

AppWindowEasyResizeWindowTargeter::AppWindowEasyResizeWindowTargeter(
    aura::Window* aura_window,
    const gfx::Insets& insets,
    ui::BaseWindow* native_app_window)
    : wm::EasyResizeWindowTargeter(aura_window, insets, insets),
      native_app_window_(native_app_window) {
}

AppWindowEasyResizeWindowTargeter::~AppWindowEasyResizeWindowTargeter() {
}

bool AppWindowEasyResizeWindowTargeter::EventLocationInsideBounds(
    aura::Window* window,
    const ui::LocatedEvent& event) const {
  // EasyResizeWindowTargeter intercepts events landing at the edges of the
  // window. Since maximized and fullscreen windows can't be resized anyway,
  // skip EasyResizeWindowTargeter so that the web contents receive all mouse
  // events.
  if (native_app_window_->IsMaximized() || native_app_window_->IsFullscreen())
    return WindowTargeter::EventLocationInsideBounds(window, event);
  else
    return EasyResizeWindowTargeter::EventLocationInsideBounds(window, event);
}

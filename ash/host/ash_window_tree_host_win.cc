// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host.h"

#include "base/win/windows_version.h"

namespace ash {
namespace {

AshWindowTreeHostWin::AshWindowTreeHostWin(const gfx::Rect& initial_bounds)
    : AshWindowTreeHostPlatform(initial_bounds),
      fullscreen_(false),
      saved_window_style_(0),
      saved_window_ex_style_(0) {}

AshWindowTreeHostWin::~AshWindowTreeHostWin() {}

void AshWindowTreeHostWin::ToggleFullScreen() override {
  gfx::Rect target_rect;
  gfx::AcceleratedWidget hwnd = GetAcceleratedWidget();
  if (!fullscreen_) {
    fullscreen_ = true;
    saved_window_style_ = GetWindowLong(hwnd, GWL_STYLE);
    saved_window_ex_style_ = GetWindowLong(hwnd, GWL_EXSTYLE);
    GetWindowRect(hwnd, &saved_window_rect_);
    SetWindowLong(hwnd, GWL_STYLE,
                  saved_window_style_ & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(hwnd, GWL_EXSTYLE,
                  saved_window_ex_style_ &
                      ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                        WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    target_rect = gfx::Rect(mi.rcMonitor);
  } else {
    fullscreen_ = false;
    SetWindowLong(hwnd, GWL_STYLE, saved_window_style_);
    SetWindowLong(hwnd, GWL_EXSTYLE, saved_window_ex_style_);
    target_rect = gfx::Rect(saved_window_rect_);
  }
  SetWindowPos(hwnd, NULL, target_rect.x(), target_rect.y(),
               target_rect.width(), target_rect.height(),
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void AshWindowTreeHostWin::SetBounds(const gfx::Rect& bounds) override {
  if (fullscreen_) {
    saved_window_rect_.right = saved_window_rect_.left + bounds.width();
    saved_window_rect_.bottom = saved_window_rect_.top + bounds.height();
    return;
  }
  AshWindowTreeHostPlatform::SetBounds(bounds);
}

}  // namespace ash

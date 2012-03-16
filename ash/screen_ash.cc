// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_ash.h"

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/monitor.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"

namespace ash {

// TODO(oshima): For m19, the origin of work area/monitor bounds for
// views/aura is (0,0) because it's simple and enough. Fix this when
// real multi monitor support is implemented.

namespace {
const aura::MonitorManager* GetMonitorManager() {
  return aura::Env::GetInstance()->monitor_manager();
}
}  // namespace

ScreenAsh::ScreenAsh(aura::RootWindow* root_window)
    : root_window_(root_window) {
}

ScreenAsh::~ScreenAsh() {
}

gfx::Point ScreenAsh::GetCursorScreenPointImpl() {
  return root_window_->last_mouse_location();
}

gfx::Rect ScreenAsh::GetMonitorWorkAreaNearestWindowImpl(
    gfx::NativeWindow window) {
  return GetMonitorManager()->GetMonitorNearestWindow(window)->
      GetWorkAreaBounds();
}

gfx::Rect ScreenAsh::GetMonitorAreaNearestWindowImpl(
    gfx::NativeWindow window) {
  // See the comment at the top.
  return gfx::Rect(
      GetMonitorManager()->GetMonitorNearestWindow(window)->size());
}

gfx::Rect ScreenAsh::GetMonitorWorkAreaNearestPointImpl(
    const gfx::Point& point) {
  return GetMonitorManager()->GetMonitorNearestPoint(point)->
      GetWorkAreaBounds();
}

gfx::Rect ScreenAsh::GetMonitorAreaNearestPointImpl(const gfx::Point& point) {
  // See the comment at the top.
  return gfx::Rect(GetMonitorManager()->GetMonitorNearestPoint(point)->size());
}

gfx::NativeWindow ScreenAsh::GetWindowAtCursorScreenPointImpl() {
  const gfx::Point point = GetCursorScreenPoint();
  return root_window_->GetTopWindowContainingPoint(point);
}

gfx::Size ScreenAsh::GetPrimaryMonitorSizeImpl() {
  return GetMonitorManager()->GetPrimaryMonitor()->size();
}

int ScreenAsh::GetNumMonitorsImpl() {
  return GetMonitorManager()->GetNumMonitors();
}

}  // namespace ash

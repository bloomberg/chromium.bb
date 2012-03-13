// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_ash.h"

#include "base/logging.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

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
  return GetWorkAreaBounds();
}

gfx::Rect ScreenAsh::GetMonitorAreaNearestWindowImpl(
    gfx::NativeWindow window) {
  return GetBounds();
}

gfx::Rect ScreenAsh::GetMonitorWorkAreaNearestPointImpl(
    const gfx::Point& point) {
  return GetWorkAreaBounds();
}

gfx::Rect ScreenAsh::GetMonitorAreaNearestPointImpl(const gfx::Point& point) {
  return GetBounds();
}

gfx::NativeWindow ScreenAsh::GetWindowAtCursorScreenPointImpl() {
  const gfx::Point point = GetCursorScreenPoint();
  return root_window_->GetTopWindowContainingPoint(point);
}

gfx::Rect ScreenAsh::GetBounds() {
  return gfx::Rect(root_window_->bounds().size());
}

gfx::Rect ScreenAsh::GetWorkAreaBounds() {
  gfx::Rect bounds(GetBounds());
  bounds.Inset(work_area_insets_);
  return bounds;
}

gfx::Size ScreenAsh::GetPrimaryMonitorSizeImpl() {
  return GetMonitorWorkAreaNearestPoint(gfx::Point()).size();
}

int ScreenAsh::GetNumMonitorsImpl() {
  return 1;
}

}  // namespace ash

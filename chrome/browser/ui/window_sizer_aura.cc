// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer.h"

#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

// TODO(oshima): Get This from WindowManager
const int WindowSizer::kWindowTilePixels = 10;

// An implementation of WindowSizer::MonitorInfoProvider that gets the actual
// monitor information from X via GDK.
class DefaultMonitorInfoProvider : public WindowSizer::MonitorInfoProvider {
 public:
  DefaultMonitorInfoProvider() { }

  virtual gfx::Rect GetPrimaryMonitorWorkArea() const {
    return gfx::Screen::GetMonitorWorkAreaNearestPoint(
        gfx::Point(0, 0));
  }

  virtual gfx::Rect GetPrimaryMonitorBounds() const {
    aura::Window* desktop_window = aura::Desktop::GetInstance()->window();
    return desktop_window->bounds();
  }

  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const {
    return gfx::Screen::GetMonitorWorkAreaNearestPoint(
        match_rect.origin());
  }

  virtual gfx::Point GetBoundsOffsetMatching(
      const gfx::Rect& match_rect) const {
    return GetMonitorWorkAreaMatching(match_rect).origin();
  }

  void UpdateWorkAreas() {
    work_areas_.clear();
    work_areas_.push_back(GetPrimaryMonitorBounds());
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(DefaultMonitorInfoProvider);
};

// static
WindowSizer::MonitorInfoProvider*
WindowSizer::CreateDefaultMonitorInfoProvider() {
  return new DefaultMonitorInfoProvider();
}

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size) {
  // TODO(oshima):This is used to control panel/popups, and this may
  // not be needed on aura environment as they must be controlled by
  // WM.
  NOTIMPLEMENTED();
  return gfx::Point();
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer.h"

#include "base/compiler_specific.h"
#include "ui/gfx/screen.h"

// This doesn't matter for aura, which has different tiling.
// static
const int WindowSizer::kWindowTilePixels = 10;

// An implementation of WindowSizer::MonitorInfoProvider. This assumes a single
// monitor, which is currently the case.
class DefaultMonitorInfoProvider : public WindowSizer::MonitorInfoProvider {
 public:
  DefaultMonitorInfoProvider() {}

  virtual gfx::Rect GetPrimaryMonitorWorkArea() const OVERRIDE {
    return gfx::Screen::GetMonitorWorkAreaNearestPoint(gfx::Point());
  }

  virtual gfx::Rect GetPrimaryMonitorBounds() const OVERRIDE {
    return gfx::Screen::GetMonitorAreaNearestPoint(gfx::Point());
  }

  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const OVERRIDE {
    return gfx::Screen::GetMonitorWorkAreaNearestPoint(gfx::Point());
  }

  virtual void UpdateWorkAreas() OVERRIDE {
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
  // TODO(oshima):This is used to control panel/popups, and this may not be
  // needed on aura environment as they must be controlled by WM.
  return gfx::Point();
}

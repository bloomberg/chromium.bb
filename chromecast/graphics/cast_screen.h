// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_SCREEN_H_
#define CHROMECAST_GRAPHICS_CAST_SCREEN_H_

#include "base/macros.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace chromecast {

namespace shell {
class CastBrowserMainParts;
}  // namespace shell

// CastScreen is Chromecast's own minimal implementation of gfx::Screen.
// Right now, it almost exactly duplicates the behavior of aura's TestScreen
// class for necessary methods. The instantiation of CastScreen occurs in
// CastBrowserMainParts, where its ownership is assigned to CastBrowserProcess.
// To then subsequently access CastScreen, see CastBrowerProcess.
class CastScreen : public gfx::Screen {
 public:
  ~CastScreen() override;

  // Updates the primary display size.
  void UpdateDisplaySize(const gfx::Size& size);

  // gfx::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  gfx::NativeWindow GetWindowUnderCursor() override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<gfx::Display> GetAllDisplays() const override;
  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  gfx::Display GetPrimaryDisplay() const override;
  void AddObserver(gfx::DisplayObserver* observer) override;
  void RemoveObserver(gfx::DisplayObserver* observer) override;

 private:
  CastScreen();

  gfx::Display display_;

  friend class shell::CastBrowserMainParts;

  DISALLOW_COPY_AND_ASSIGN(CastScreen);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_SCREEN_H_

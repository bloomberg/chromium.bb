// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_AURA_SCREEN_MOJO_H_
#define MANDOLINE_UI_AURA_SCREEN_MOJO_H_

#include "base/macros.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace gfx {
class Rect;
class Transform;
}

namespace mandoline {

// A minimal implementation of gfx::Screen for mojo.
class ScreenMojo : public gfx::Screen {
 public:
  static ScreenMojo* Create();
  ~ScreenMojo() override;

 protected:
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
  explicit ScreenMojo(const gfx::Rect& screen_bounds);

  gfx::Display display_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMojo);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_SCREEN_MOJO_H_

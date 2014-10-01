// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_AURA_SCREEN_MOJO_H_
#define MOJO_AURA_SCREEN_MOJO_H_

#include "base/macros.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace gfx {
class Rect;
class Transform;
}

namespace mojo {

// A minimal implementation of gfx::Screen for mojo.
class ScreenMojo : public gfx::Screen {
 public:
  static ScreenMojo* Create();
  virtual ~ScreenMojo();

 protected:
  // gfx::Screen overrides:
  virtual bool IsDIPEnabled() override;
  virtual gfx::Point GetCursorScreenPoint() override;
  virtual gfx::NativeWindow GetWindowUnderCursor() override;
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      override;
  virtual int GetNumDisplays() const override;
  virtual std::vector<gfx::Display> GetAllDisplays() const override;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const override;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  virtual gfx::Display GetPrimaryDisplay() const override;
  virtual void AddObserver(gfx::DisplayObserver* observer) override;
  virtual void RemoveObserver(gfx::DisplayObserver* observer) override;

 private:
  explicit ScreenMojo(const gfx::Rect& screen_bounds);

  gfx::Display display_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMojo);
};

}  // namespace mojo

#endif  // MOJO_AURA_SCREEN_MOJO_H_

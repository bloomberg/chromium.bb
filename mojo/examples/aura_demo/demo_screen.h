// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_AURA_DEMO_DEMO_SCREEN_H_
#define MOJO_EXAMPLES_AURA_DEMO_DEMO_SCREEN_H_

#include "base/compiler_specific.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace gfx {
class Rect;
class Transform;
}

namespace mojo {
namespace examples {

// A minimal, testing Aura implementation of gfx::Screen.
class DemoScreen : public gfx::Screen {
 public:
  static DemoScreen* Create();
  virtual ~DemoScreen();

 protected:
  // gfx::Screen overrides:
  virtual bool IsDIPEnabled() OVERRIDE;
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual gfx::NativeWindow GetWindowUnderCursor() OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      OVERRIDE;
  virtual int GetNumDisplays() const OVERRIDE;
  virtual std::vector<gfx::Display> GetAllDisplays() const OVERRIDE;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const OVERRIDE;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE;
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE;
  virtual void AddObserver(gfx::DisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(gfx::DisplayObserver* observer) OVERRIDE;

 private:
  explicit DemoScreen(const gfx::Rect& screen_bounds);

  gfx::Display display_;

  DISALLOW_COPY_AND_ASSIGN(DemoScreen);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_AURA_DEMO_DEMO_SCREEN_H_

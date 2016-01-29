// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_UI_BLIMP_SCREEN_H_
#define BLIMP_ENGINE_APP_UI_BLIMP_SCREEN_H_

#include <vector>

#include "base/macros.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace blimp {
namespace engine {

// Presents the client's single screen.
class BlimpScreen : public gfx::Screen {
 public:
  BlimpScreen();
  ~BlimpScreen() override;

  // Updates the size reported by the primary display.
  void UpdateDisplayScaleAndSize(float scale, const gfx::Size& size);

  // gfx::Screen implementation.
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
  gfx::Display display_;

  DISALLOW_COPY_AND_ASSIGN(BlimpScreen);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_UI_BLIMP_SCREEN_H_

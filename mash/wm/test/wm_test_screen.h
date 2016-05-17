// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_WM_TEST_SCREEN_H_
#define MASH_WM_WM_TEST_SCREEN_H_

#include "ui/display/screen.h"
#include "ui/views/mus/display_list.h"

namespace mash {
namespace wm {

// Screen implementation used by tests for the windowmanager. Does not talk
// to mus. Tests must keep the list of Displays in sync manually.
class WmTestScreen : public display::Screen {
 public:
  WmTestScreen();
  ~WmTestScreen() override;

  const views::DisplayList& display_list() const { return display_list_; }
  views::DisplayList* display_list() { return &display_list_; }

 private:
  // display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  int GetNumDisplays() const override;
  std::vector<display::Display> GetAllDisplays() const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

  views::DisplayList display_list_;

  DISALLOW_COPY_AND_ASSIGN(WmTestScreen);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_WM_TEST_SCREEN_H_

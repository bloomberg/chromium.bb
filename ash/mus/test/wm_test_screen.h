// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_TEST_WM_TEST_SCREEN_H_
#define ASH_MUS_TEST_WM_TEST_SCREEN_H_

#include "ui/display/screen_base.h"

namespace ash {
namespace mus {

// Screen implementation used by tests for the windowmanager. Does not talk
// to mus. Tests must keep the list of Displays in sync manually.
class WmTestScreen : public display::ScreenBase {
 public:
  WmTestScreen();
  ~WmTestScreen() override;

 private:
  // display::ScreenBase:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;

  DISALLOW_COPY_AND_ASSIGN(WmTestScreen);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_TEST_WM_TEST_SCREEN_H_

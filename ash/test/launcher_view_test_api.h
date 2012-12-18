// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_LAUNCHER_VIEW_TEST_API_H_
#define ASH_TEST_LAUNCHER_VIEW_TEST_API_H_

#include "base/basictypes.h"

namespace gfx {
class Rect;
}

namespace ash {

namespace internal {
class LauncherButton;
class LauncherView;
}

namespace test {

// Use the api in this class to test LauncherView.
class LauncherViewTestAPI {
 public:
  explicit LauncherViewTestAPI(internal::LauncherView* launcher_view);
  ~LauncherViewTestAPI();

  // Number of icons displayed.
  int GetButtonCount();

  // Retrieve the button at |index|.
  internal::LauncherButton* GetButton(int index);

  // Last visible button index.
  int GetLastVisibleIndex();

  // Gets current/ideal bounds for button at |index|.
  const gfx::Rect& GetBoundsByIndex(int index);
  const gfx::Rect& GetIdealBoundsByIndex(int index);

  // Returns true if overflow button is visible.
  bool IsOverflowButtonVisible();

  // Makes launcher view show its overflow bubble.
  void ShowOverflowBubble();

  // Sets animation duration in milliseconds for test.
  void SetAnimationDuration(int duration_ms);

  // Runs message loop and waits until all add/remove animations are done.
  void RunMessageLoopUntilAnimationsDone();

 private:
  internal::LauncherView* launcher_view_;

  DISALLOW_COPY_AND_ASSIGN(LauncherViewTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_LAUNCHER_VIEW_TEST_API_H_

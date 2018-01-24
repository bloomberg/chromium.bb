// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_

#include "base/macros.h"

class FullscreenController;

// A helper object that increments FullscrenController's disabled counter for
// its entire lifetime. Since the disabling is happening inside an animation
// block, any UI changes related to Fullscreen being disabled will be animated.
class AnimatedScopedFullscreenDisabler {
 public:
  explicit AnimatedScopedFullscreenDisabler(FullscreenController* controller);
  ~AnimatedScopedFullscreenDisabler();

 private:
  // The FullscreenController being disabled by this object.
  FullscreenController* controller_;

  DISALLOW_COPY_AND_ASSIGN(AnimatedScopedFullscreenDisabler);
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_

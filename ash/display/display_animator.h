// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ANIMATOR_H_
#define ASH_DISPLAY_DISPLAY_ANIMATOR_H_

#include <map>

#include "ash/ash_export.h"
#include "base/callback.h"

namespace ash {

// Interface class for animating display changes.
class ASH_EXPORT DisplayAnimator {
 public:
  virtual ~DisplayAnimator() {}

  // Starts the fade-out animation for the all root windows. It will
  // call |callback| once all of the animations have finished.
  virtual void StartFadeOutAnimation(base::Closure callback) = 0;

  // Starts the animation to clear the fade-out animation effect
  // for the all root windows.
  virtual void StartFadeInAnimation() = 0;

 private:
  DISALLOW_ASSIGN(DisplayAnimator);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ANIMATOR_H_

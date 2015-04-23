// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_
#define ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/gfx/display.h"

namespace ash {

// Utility to perform a screen rotation with an animation.
class ASH_EXPORT ScreenRotationAnimator {
 public:
  explicit ScreenRotationAnimator(int64 display_id);
  ~ScreenRotationAnimator();

  // Rotates |display_| to the |new_rotation| orientation, for the given
  // |source|. The rotation will also become active.
  void Rotate(gfx::Display::Rotation new_rotation,
              gfx::Display::RotationSource source);

 private:
  // The id of the display to rotate.
  int64 display_id_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationAnimator);
};

}  // namespace ash

#endif  // ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_

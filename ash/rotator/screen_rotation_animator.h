// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_
#define ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/display/display.h"

namespace ash {

// Utility to perform a screen rotation with an animation.
class ASH_EXPORT ScreenRotationAnimator {
 public:
  explicit ScreenRotationAnimator(int64_t display_id);
  ~ScreenRotationAnimator();

  // Returns true if the screen rotation animation can be completed
  // successfully. For example an animation is not possible if |display_id_|
  // specifies a display::Display that is not currently active. See
  // www.crbug.com/479503.
  bool CanAnimate() const;

  // Rotates the display::Display specified by |display_id_| to the
  // |new_rotation|
  // orientation, for the given |source|. The rotation will also become active.
  // Clients should only call |Rotate()| if |CanAnimate()| returns true.
  void Rotate(display::Display::Rotation new_rotation,
              display::Display::RotationSource source);

 private:
  // The id of the display to rotate.
  int64_t display_id_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationAnimator);
};

}  // namespace ash

#endif  // ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_

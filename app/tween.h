// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_TWEEN_H_
#define APP_TWEEN_H_
#pragma once

#include "base/logging.h"

namespace gfx {
class Rect;
}

class Tween {
 public:
  enum Type {
    LINEAR,        // Linear.
    EASE_OUT,      // Fast in, slow out (default).
    EASE_IN,       // Slow in, fast out.
    EASE_IN_OUT,   // Slow in and out, fast in the middle.
    FAST_IN_OUT,   // Fast in and out, slow in the middle.
    EASE_OUT_SNAP, // Fast in, slow out, snap to final value.
    ZERO,          // Returns a value of 0 always.
  };

  // Returns the value based on the tween type. |state| is from 0-1.
  static double CalculateValue(Type type, double state);

  // Conveniences for getting a value between a start and end point.
  static double ValueBetween(double value, double start, double target);
  static int ValueBetween(double value, int start, int target);
  static gfx::Rect ValueBetween(double value,
                                const gfx::Rect& start_bounds,
                                const gfx::Rect& target_bounds);

 private:
  Tween();
  ~Tween();

  DISALLOW_COPY_AND_ASSIGN(Tween);
};

#endif  // APP_TWEEN_H_

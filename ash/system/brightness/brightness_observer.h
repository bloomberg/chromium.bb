// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_OBSERVER_H_
#define ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT BrightnessObserver {
 public:
  virtual ~BrightnessObserver() {}

  // |percent| is in the range [0.0, 100.0].
  virtual void OnBrightnessChanged(double percent, bool user_initiated) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_BRIGHTNESS_OBSERVER_H_

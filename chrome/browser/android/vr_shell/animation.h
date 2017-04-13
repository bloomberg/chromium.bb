// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_ANIMATION_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_ANIMATION_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"

namespace vr_shell {

namespace easing {
class Easing;
}

// Describes the characteristics of a transition from an initial set of values
// to a final set, with timing and interpolation information.  Other classes use
// this information to animate UI element location, size, and other properties.
class Animation {
 public:
  enum Property {
    SIZE = 0,
    TRANSLATION,
    SCALE,
    ROTATION,
    OPACITY,
  };

  Animation(int id,
            Property property,
            std::unique_ptr<easing::Easing> easing,
            std::vector<float> from,
            std::vector<float> to,
            const base::TimeTicks& start,
            const base::TimeDelta& duration);
  ~Animation();

  int id;
  Property property;
  std::unique_ptr<easing::Easing> easing;
  std::vector<float> from;
  std::vector<float> to;
  base::TimeTicks start;
  base::TimeDelta duration;

 private:
  DISALLOW_COPY_AND_ASSIGN(Animation);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_ANIMATION_H_

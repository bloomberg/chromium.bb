// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_ANIMATION_UTILS_H_
#define CHROME_BROWSER_VR_TEST_ANIMATION_UTILS_H_

#include "cc/animation/animation.h"
#include "cc/animation/keyframed_animation_curve.h"

namespace vr {

std::unique_ptr<cc::Animation> CreateTransformAnimation(
    int id,
    int group,
    const cc::TransformOperations& from,
    const cc::TransformOperations& to,
    base::TimeDelta duration);

std::unique_ptr<cc::Animation> CreateBoundsAnimation(int id,
                                                     int group,
                                                     const gfx::SizeF& from,
                                                     const gfx::SizeF& to,
                                                     base::TimeDelta duration);

base::TimeTicks UsToTicks(uint64_t us);
base::TimeDelta UsToDelta(uint64_t us);

base::TimeTicks MsToTicks(uint64_t us);
base::TimeDelta MsToDelta(uint64_t us);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_ANIMATION_UTILS_H_

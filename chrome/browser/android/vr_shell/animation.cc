// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/animation.h"

#include <utility>

#include "chrome/browser/android/vr_shell/easing.h"

namespace vr_shell {

Animation::Animation(int id,
                     Property property,
                     std::unique_ptr<easing::Easing> easing,
                     std::vector<float> from,
                     std::vector<float> to,
                     int64_t start,
                     int64_t duration)
    : id(id),
      property(property),
      easing(std::move(easing)),
      from(from),
      to(to),
      start(start),
      duration(duration) {}

Animation::~Animation() {}

}  // namespace vr_shell

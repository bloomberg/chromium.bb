// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TARGET_PROPERTY_H_
#define CC_TREES_TARGET_PROPERTY_H_

#include <bitset>

#include "cc/cc_export.h"

namespace cc {

namespace TargetProperty {

enum Type {
  TRANSFORM = 0,
  OPACITY,
  FILTER,
  SCROLL_OFFSET,
  BACKGROUND_COLOR,
  BOUNDS,
  VISIBILITY,
  // These sentinels must be last
  FIRST_TARGET_PROPERTY = TRANSFORM,
  LAST_TARGET_PROPERTY = VISIBILITY,
};

CC_EXPORT const char* GetName(TargetProperty::Type property);

}  // namespace TargetProperty

// A set of target properties. TargetProperty must be 0-based enum.
using TargetProperties = std::bitset<TargetProperty::LAST_TARGET_PROPERTY + 1>;

}  // namespace cc

#endif  // CC_TREES_TARGET_PROPERTY_H_

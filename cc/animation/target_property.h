// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_TARGET_PROPERTY_H_
#define CC_ANIMATION_TARGET_PROPERTY_H_

namespace cc {

namespace TargetProperty {

enum Type {
  TRANSFORM = 0,
  OPACITY,
  FILTER,
  SCROLL_OFFSET,
  BACKGROUND_COLOR,
  // This sentinel must be last.
  LAST_TARGET_PROPERTY = BACKGROUND_COLOR
};

const char* GetName(TargetProperty::Type property);

}  // namespace TargetProperty

}  // namespace cc

#endif  // CC_ANIMATION_TARGET_PROPERTY_H_

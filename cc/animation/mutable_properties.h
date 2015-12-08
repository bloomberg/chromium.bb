// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_MUTABLE_PROPERTIES_H_
#define CC_ANIMATION_MUTABLE_PROPERTIES_H_

namespace cc {

enum MutableProperty {
  kMutablePropertyNone = 0,
  kMutablePropertyOpacity = 1 << 0,
  kMutablePropertyScrollLeft = 1 << 1,
  kMutablePropertyScrollTop = 1 << 2,
  kMutablePropertyTransform = 1 << 3,
};

}  // namespace cc

#endif  // CC_ANIMATION_MUTABLE_PROPERTIES_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_PROPERTY_ANIMATION_STATE_H_
#define CC_ANIMATION_PROPERTY_ANIMATION_STATE_H_

#include "cc/base/cc_export.h"

namespace cc {

struct CC_EXPORT PropertyAnimationState {
  bool currently_running_for_active_elements = false;
  bool currently_running_for_pending_elements = false;
  bool potentially_animating_for_active_elements = false;
  bool potentially_animating_for_pending_elements = false;

  bool operator==(const PropertyAnimationState& other) const;
  bool operator!=(const PropertyAnimationState& other) const;

  PropertyAnimationState& operator|=(const PropertyAnimationState& other);
  PropertyAnimationState& operator^=(const PropertyAnimationState& other);

  bool IsValid() const;

  void Clear();
};

PropertyAnimationState operator^(const PropertyAnimationState& lhs,
                                 const PropertyAnimationState& rhs);

}  // namespace cc

#endif  // CC_ANIMATION_PROPERTY_ANIMATION_STATE_H_

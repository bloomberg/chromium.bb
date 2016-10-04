// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/property_animation_state.h"

#include "base/logging.h"

namespace cc {

bool PropertyAnimationState::operator==(
    const PropertyAnimationState& other) const {
  return currently_running_for_active_elements ==
             other.currently_running_for_active_elements &&
         currently_running_for_pending_elements ==
             other.currently_running_for_pending_elements &&
         potentially_animating_for_active_elements ==
             other.potentially_animating_for_active_elements &&
         potentially_animating_for_pending_elements ==
             other.potentially_animating_for_pending_elements;
}

bool PropertyAnimationState::operator!=(
    const PropertyAnimationState& other) const {
  return !operator==(other);
}

PropertyAnimationState& PropertyAnimationState::operator|=(
    const PropertyAnimationState& other) {
  currently_running_for_active_elements |=
      other.currently_running_for_active_elements;
  currently_running_for_pending_elements |=
      other.currently_running_for_pending_elements;
  potentially_animating_for_active_elements |=
      other.potentially_animating_for_active_elements;
  potentially_animating_for_pending_elements |=
      other.potentially_animating_for_pending_elements;

  return *this;
}

PropertyAnimationState& PropertyAnimationState::operator^=(
    const PropertyAnimationState& other) {
  currently_running_for_active_elements ^=
      other.currently_running_for_active_elements;
  currently_running_for_pending_elements ^=
      other.currently_running_for_pending_elements;
  potentially_animating_for_active_elements ^=
      other.potentially_animating_for_active_elements;
  potentially_animating_for_pending_elements ^=
      other.potentially_animating_for_pending_elements;

  return *this;
}

PropertyAnimationState operator^(const PropertyAnimationState& lhs,
                                 const PropertyAnimationState& rhs) {
  PropertyAnimationState result = lhs;
  result ^= rhs;
  return result;
}

bool PropertyAnimationState::IsValid() const {
  // currently_running must be a subset for potentially_animating.
  // currently <= potentially.
  return currently_running_for_active_elements <=
             potentially_animating_for_active_elements &&
         currently_running_for_pending_elements <=
             potentially_animating_for_pending_elements;
}

void PropertyAnimationState::Clear() {
  currently_running_for_active_elements = false;
  currently_running_for_pending_elements = false;
  potentially_animating_for_active_elements = false;
  potentially_animating_for_pending_elements = false;
}

}  // namespace cc

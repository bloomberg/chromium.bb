// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animations.h"

#include "cc/animation/animation_host.h"

namespace cc {

ScrollOffsetAnimationUpdate::ScrollOffsetAnimationUpdate(Type type,
                                                         ElementId element_id)
    : type_(type), element_id_(element_id) {}

ScrollOffsetAnimations::ScrollOffsetAnimations(AnimationHost* animation_host)
    : animation_host_(animation_host) {}

ScrollOffsetAnimations::~ScrollOffsetAnimations() {}

void ScrollOffsetAnimations::AddUpdate(ScrollOffsetAnimationUpdate update) {
  queue_.push_back(update);
  animation_host_->SetNeedsCommit();
}

bool ScrollOffsetAnimations::HasUpdatesForTesting() const {
  return !queue_.empty();
}

void ScrollOffsetAnimations::PushPropertiesTo(
    ScrollOffsetAnimationsImpl* animations) {
  DCHECK(animations);
  if (queue_.empty())
    return;

  for (auto& update : queue_) {
    switch (update.type_) {
      case ScrollOffsetAnimationUpdate::Type::SCROLL_OFFSET_CHANGED:
        animations->ScrollAnimationApplyAdjustment(update.element_id_,
                                                   update.adjustment_);
    }
  }
  queue_.clear();
}

}  // namespace cc

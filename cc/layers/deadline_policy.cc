// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/deadline_policy.h"

namespace cc {

// static
DeadlinePolicy DeadlinePolicy::UseExistingDeadline() {
  return DeadlinePolicy(Type::kUseExistingDeadline);
}

// static
DeadlinePolicy DeadlinePolicy::UseDefaultDeadline() {
  return DeadlinePolicy(Type::kUseDefaultDeadline);
}

// static
DeadlinePolicy DeadlinePolicy::UseSpecifiedDeadline(
    uint32_t deadline_in_frames) {
  return DeadlinePolicy(deadline_in_frames);
}

DeadlinePolicy::DeadlinePolicy(Type policy_type)
    : policy_type_(policy_type), deadline_in_frames_(base::nullopt) {
  DCHECK_NE(Type::kUseSpecifiedDeadline, policy_type_);
}

DeadlinePolicy::DeadlinePolicy(uint32_t deadline_in_frames)
    : policy_type_(Type::kUseSpecifiedDeadline),
      deadline_in_frames_(deadline_in_frames) {}

DeadlinePolicy::DeadlinePolicy(const DeadlinePolicy& other) = default;

}  // namespace cc

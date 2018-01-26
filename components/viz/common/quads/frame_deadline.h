// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_FRAME_DEADLINE_H_
#define COMPONENTS_VIZ_COMMON_QUADS_FRAME_DEADLINE_H_

#include "components/viz/common/viz_common_export.h"

namespace viz {

class VIZ_COMMON_EXPORT FrameDeadline {
 public:
  FrameDeadline() = default;
  FrameDeadline(uint32_t value, bool use_default_lower_bound_deadline)
      : value_(value),
        use_default_lower_bound_deadline_(use_default_lower_bound_deadline) {}

  FrameDeadline(const FrameDeadline& other) = default;

  FrameDeadline& operator=(const FrameDeadline& other) = default;

  bool operator==(const FrameDeadline& other) const {
    return other.value_ == value_ && other.use_default_lower_bound_deadline_ ==
                                         use_default_lower_bound_deadline_;
  }

  bool operator!=(const FrameDeadline& other) const {
    return !(*this == other);
  }

  uint32_t value() const { return value_; }

  bool use_default_lower_bound_deadline() const {
    return use_default_lower_bound_deadline_;
  }

 private:
  uint32_t value_ = 0u;
  bool use_default_lower_bound_deadline_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_FRAME_DEADLINE_H_

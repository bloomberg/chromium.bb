// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/inter_process_time_ticks_converter.h"

#include "base/logging.h"

namespace content {

InterProcessTimeTicksConverter::InterProcessTimeTicksConverter(
    const LocalTimeTicks& local_lower_bound,
    const LocalTimeTicks& local_upper_bound,
    const RemoteTimeTicks& remote_lower_bound,
    const RemoteTimeTicks& remote_upper_bound)
    : remote_lower_bound_(remote_lower_bound.value_),
      remote_upper_bound_(remote_upper_bound.value_) {
#define CONVERTER_DISABLED_DUE_TO_BUG_174170
#ifdef CONVERTER_DISABLED_DUE_TO_BUG_174170
  numerator_ = 1;
  denominator_ = 1;
  offset_ = 0;
  return;
#endif
  int64 target_range = local_upper_bound.value_ - local_lower_bound.value_;
  int64 source_range = remote_upper_bound.value_ - remote_lower_bound.value_;
  if (source_range <= target_range) {
    // We fit!  Just shift the midpoints to match.
    numerator_ = 1;
    denominator_ = 1;
    offset_ = ((local_upper_bound.value_ + local_lower_bound.value_) -
               (remote_upper_bound.value_ + remote_lower_bound.value_)) / 2;
    return;
  }
  // Set up scaling factors, and then deduce shift.
  numerator_ = target_range;
  denominator_ = source_range;
  // Find out what we need to shift by to make this really work.
  offset_ = local_lower_bound.value_ - Convert(remote_lower_bound.value_);
  DCHECK_GE(local_upper_bound.value_, Convert(remote_upper_bound.value_));
}

LocalTimeTicks InterProcessTimeTicksConverter::ToLocalTimeTicks(
    const RemoteTimeTicks& remote_ms) {
  DCHECK_LE(remote_lower_bound_, remote_ms.value_);
  DCHECK_GE(remote_upper_bound_, remote_ms.value_);
  RemoteTimeDelta remote_delta = remote_ms - remote_lower_bound_;
  return LocalTimeTicks(remote_lower_bound_ + offset_ +
                        ToLocalTimeDelta(remote_delta).value_);
}

LocalTimeDelta InterProcessTimeTicksConverter::ToLocalTimeDelta(
    const RemoteTimeDelta& remote_delta) {
  DCHECK_GE(remote_upper_bound_, remote_lower_bound_ + remote_delta.value_);
  return LocalTimeDelta(Convert(remote_delta.value_));
}

int64 InterProcessTimeTicksConverter::Convert(int64 value) {
  if (value <= 0) {
    return value;
  }
  return numerator_ * value / denominator_;
}

}  // namespace content

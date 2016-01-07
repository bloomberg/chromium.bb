// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_CAPTURE_FEEDBACK_SIGNAL_ACCUMULATOR_CC_
#define MEDIA_CAPTURE_FEEDBACK_SIGNAL_ACCUMULATOR_CC_

#include "media/capture/content/feedback_signal_accumulator.h"

#include <algorithm>
#include <cmath>

namespace media {

template <typename TimeType>
FeedbackSignalAccumulator<TimeType>::FeedbackSignalAccumulator(
    base::TimeDelta half_life)
    : half_life_(half_life), average_(NAN) {
  DCHECK(half_life_ > base::TimeDelta());
}

template <typename TimeType>
void FeedbackSignalAccumulator<TimeType>::Reset(double starting_value,
                                                TimeType timestamp) {
  average_ = update_value_ = prior_average_ = starting_value;
  reset_time_ = update_time_ = prior_update_time_ = timestamp;
}

template <typename TimeType>
bool FeedbackSignalAccumulator<TimeType>::Update(double value,
                                                 TimeType timestamp) {
  DCHECK(!std::isnan(average_)) << "Reset() must be called once.";

  if (timestamp < update_time_) {
    return false;  // Not in chronological order.
  } else if (timestamp == update_time_) {
    if (timestamp == reset_time_) {
      // Edge case: Multiple updates at reset timestamp.
      average_ = update_value_ = prior_average_ =
          std::max(value, update_value_);
      return true;
    }
    if (value <= update_value_)
      return true;
    update_value_ = value;
  } else {
    prior_average_ = average_;
    prior_update_time_ = update_time_;
    update_value_ = value;
    update_time_ = timestamp;
  }

  const double elapsed_us =
      static_cast<double>((update_time_ - prior_update_time_).InMicroseconds());
  const double weight = elapsed_us / (elapsed_us + half_life_.InMicroseconds());
  average_ = weight * update_value_ + (1.0 - weight) * prior_average_;
  DCHECK(std::isfinite(average_));

  return true;
}

template class MEDIA_EXPORT FeedbackSignalAccumulator<base::TimeDelta>;
template class MEDIA_EXPORT FeedbackSignalAccumulator<base::TimeTicks>;
}  // namespace media

#endif  // MEDIA_CAPTURE_FEEDBACK_SIGNAL_ACCUMULATOR_CC_

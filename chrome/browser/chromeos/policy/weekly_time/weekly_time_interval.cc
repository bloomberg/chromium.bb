// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/weekly_time/weekly_time_interval.h"
#include "base/time/time.h"

namespace policy {

WeeklyTimeInterval::WeeklyTimeInterval(const WeeklyTime& start,
                                       const WeeklyTime& end)
    : start_(start), end_(end) {
  DCHECK_GT(start.GetDurationTo(end), base::TimeDelta());
}

WeeklyTimeInterval::WeeklyTimeInterval(const WeeklyTimeInterval& rhs) = default;

WeeklyTimeInterval& WeeklyTimeInterval::operator=(
    const WeeklyTimeInterval& rhs) = default;

std::unique_ptr<base::DictionaryValue> WeeklyTimeInterval::ToValue() const {
  auto interval = std::make_unique<base::DictionaryValue>();
  interval->SetDictionary("start", start_.ToValue());
  interval->SetDictionary("end", end_.ToValue());
  return interval;
}

bool WeeklyTimeInterval::Contains(const WeeklyTime& w) const {
  if (w.GetDurationTo(end_).is_zero())
    return false;
  base::TimeDelta interval_duration = start_.GetDurationTo(end_);
  return start_.GetDurationTo(w) + w.GetDurationTo(end_) == interval_duration;
}

}  // namespace policy

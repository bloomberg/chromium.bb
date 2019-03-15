// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/consistency_golden_converter.h"

namespace chromeos {
namespace time_limit_consistency_utils {

void AddWindowLimitEntryToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenEffectiveDay effective_day,
    int starts_at_hour,
    int starts_at_minute,
    int ends_at_hour,
    int ends_at_minute) {
  time_limit_consistency::ConsistencyGoldenWindowLimitEntry* window =
      golden_input->add_window_limits();
  window->mutable_starts_at()->set_hour(starts_at_hour);
  window->mutable_starts_at()->set_minute(starts_at_minute);
  window->mutable_ends_at()->set_hour(ends_at_hour);
  window->mutable_ends_at()->set_minute(ends_at_minute);
  window->set_effective_day(effective_day);
}

void AddUsageLimitEntryToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenEffectiveDay effective_day,
    int usage_quota_mins) {
  time_limit_consistency::ConsistencyGoldenUsageLimitEntry* usage_limit =
      golden_input->add_usage_limits();
  usage_limit->set_usage_quota_mins(usage_quota_mins);
  usage_limit->set_effective_day(effective_day);
}

}  // namespace time_limit_consistency_utils
}  // namespace chromeos

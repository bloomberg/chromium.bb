// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities to be used by the consistency golden converter unit tests.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_CONSISTENCY_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_CONSISTENCY_TEST_UTILS_H_

#include "chrome/browser/chromeos/child_accounts/time_limit_consistency_test/goldens/consistency_golden.pb.h"

namespace chromeos {
namespace time_limit_consistency_utils {

// Adds a time window limit entry to the provided ConsistencyGoldenInput.
void AddWindowLimitEntryToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenEffectiveDay effective_day,
    int starts_at_hour,
    int starts_at_minute,
    int ends_at_hour,
    int ends_at_minute);

// Adds a usage limit entry to the provided ConsistencyGoldenInput.
void AddUsageLimitEntryToGoldenInput(
    time_limit_consistency::ConsistencyGoldenInput* golden_input,
    time_limit_consistency::ConsistencyGoldenEffectiveDay effective_day,
    int usage_quota_mins);

}  // namespace time_limit_consistency_utils
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_CONSISTENCY_TEST_UTILS_H_

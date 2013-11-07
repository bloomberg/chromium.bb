// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_VARIATIONS_VARIATIONS_UTIL_H_
#define CHROME_COMMON_METRICS_VARIATIONS_VARIATIONS_UTIL_H_

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "components/variations/variations_associated_data.h"

namespace chrome_variations {

// Fills the supplied vector |name_group_ids| (which must be empty when called)
// with unique ActiveGroupIds for each Field Trial that has a chosen group.
// Field Trials for which a group has not been chosen yet are NOT returned in
// this list.
void GetFieldTrialActiveGroupIds(std::vector<ActiveGroupId>* name_group_ids);

// Fills the supplied vector |output| (which must be empty when called) with
// unique string representations of ActiveGroupIds for each Field Trial that
// has a chosen group. The strings are formatted as "<TrialName>-<GroupName>",
// with the names as hex strings. Field Trials for which a group has not been
// chosen yet are NOT returned in this list.
void GetFieldTrialActiveGroupIdsAsStrings(std::vector<std::string>* output);

// Get the current set of chosen FieldTrial groups (aka variations) and send
// them to the child process logging module so it can save it for crash dumps.
void SetChildProcessLoggingVariationList();

// Expose some functions for testing. These functions just wrap functionality
// that is implemented above.
namespace testing {

// Clears all of the mapped associations.
void ClearAllVariationIDs();

void TestGetFieldTrialActiveGroupIds(
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids);

}  // namespace testing

}  // namespace chrome_variations

#endif  // CHROME_COMMON_METRICS_VARIATIONS_VARIATIONS_UTIL_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/variations/variations_util.h"

#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/crash_keys.h"

namespace chrome_variations {

namespace {

// Populates |name_group_ids| based on |active_groups|.
void GetFieldTrialActiveGroupIdsForActiveGroups(
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  for (base::FieldTrial::ActiveGroups::const_iterator it =
       active_groups.begin(); it != active_groups.end(); ++it) {
    name_group_ids->push_back(MakeActiveGroupId(it->trial_name,
                                                it->group_name));
  }
}

}  // namespace

void GetFieldTrialActiveGroupIds(
    std::vector<ActiveGroupId>* name_group_ids) {
  DCHECK(name_group_ids->empty());
  // A note on thread safety: Since GetActiveFieldTrialGroups() is thread
  // safe, and we operate on a separate list of that data, this function is
  // technically thread safe as well, with respect to the FieldTrialList data.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  GetFieldTrialActiveGroupIdsForActiveGroups(active_groups,
                                             name_group_ids);
}

void GetFieldTrialActiveGroupIdsAsStrings(
    std::vector<std::string>* output) {
  DCHECK(output->empty());
  std::vector<ActiveGroupId> name_group_ids;
  GetFieldTrialActiveGroupIds(&name_group_ids);
  for (size_t i = 0; i < name_group_ids.size(); ++i) {
    output->push_back(base::StringPrintf(
        "%x-%x", name_group_ids[i].name, name_group_ids[i].group));
  }
}

void SetChildProcessLoggingVariationList() {
  std::vector<std::string> experiment_strings;
  GetFieldTrialActiveGroupIdsAsStrings(&experiment_strings);
  crash_keys::SetVariationsList(experiment_strings);
}

// Functions below are exposed for testing explicitly behind this namespace.
// They simply wrap existing functions in this file.
namespace testing {

void TestGetFieldTrialActiveGroupIds(
    const base::FieldTrial::ActiveGroups& active_groups,
    std::vector<ActiveGroupId>* name_group_ids) {
  GetFieldTrialActiveGroupIdsForActiveGroups(active_groups,
                                             name_group_ids);
}

}  // namespace testing

}  // namespace chrome_variations

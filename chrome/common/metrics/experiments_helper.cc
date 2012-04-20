// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/experiments_helper.h"

#include <vector>

#include "base/metrics/field_trial.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/child_process_logging.h"

namespace ExperimentsHelper {

void SetChildProcessLoggingExperimentList() {
  std::vector<base::FieldTrial::NameGroupId> name_group_ids;
  base::FieldTrialList::GetFieldTrialNameGroupIds(&name_group_ids);
  std::vector<string16> experiment_strings(name_group_ids.size());
  for (size_t i = 0; i < name_group_ids.size(); ++i) {
    // Wish there was a StringPrintf for string16... :-(
    experiment_strings[i] = WideToUTF16(base::StringPrintf(
        L"%x-%x", name_group_ids[i].name, name_group_ids[i].group));
  }
  child_process_logging::SetExperimentList(experiment_strings);
}

}  // namespace ExperimentsHelper

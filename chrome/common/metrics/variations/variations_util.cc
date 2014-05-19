// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/variations/variations_util.h"

#include "chrome/common/child_process_logging.h"
#include "chrome/common/crash_keys.h"
#include "components/variations/active_field_trials.h"

namespace chrome_variations {

void SetChildProcessLoggingVariationList() {
  std::vector<std::string> experiment_strings;
  variations::GetFieldTrialActiveGroupIdsAsStrings(&experiment_strings);
  crash_keys::SetVariationsList(experiment_strings);
}

}  // namespace chrome_variations

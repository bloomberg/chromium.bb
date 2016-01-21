// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_field_trials.h"

#include "base/metrics/field_trial.h"

void SetupFieldTrials(const base::CommandLine& command_line,
                      const base::Time& install_time) {
  // Activate the iOS tab eviction dynamic field trials.
  base::FieldTrialList::FindValue("TabEviction");
}

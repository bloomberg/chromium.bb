// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/field_trial_synchronizer.h"

#include <vector>

#include "base/logging.h"
#include "components/variations/variations_util.h"

namespace ios {

FieldTrialSynchronizer::FieldTrialSynchronizer() {
  base::FieldTrialList::AddObserver(this);
  SynchronizeCrashKeyExperimentList();
}

FieldTrialSynchronizer::~FieldTrialSynchronizer() {
  base::FieldTrialList::RemoveObserver(this);
}

void FieldTrialSynchronizer::OnFieldTrialGroupFinalized(
    const std::string& field_trial_name,
    const std::string& group_name) {
  CHECK(!field_trial_name.empty() && !group_name.empty());
  SynchronizeCrashKeyExperimentList();
}

void FieldTrialSynchronizer::SynchronizeCrashKeyExperimentList() {
  variations::SetVariationListCrashKeys();
}

}  // namespace ios

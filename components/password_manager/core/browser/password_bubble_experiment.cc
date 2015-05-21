// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "components/password_manager/core/browser/password_manager_util.h"

namespace password_bubble_experiment {
namespace {

const char kBrandingExperimentName[] = "PasswordBranding";
const char kSmartLockBrandingGroupName[] = "SmartLockBranding";

}  // namespace

void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason) {
  // TODO(vasilii): store the statistics and consider merging with
  // password_manager_metrics_util.*.
}

bool IsSmartLockBrandingEnabled(const sync_driver::SyncService* sync_service) {
  return password_manager_util::GetPasswordSyncState(sync_service) ==
             password_manager::SYNCING_NORMAL_ENCRYPTION &&
         base::FieldTrialList::FindFullName(kBrandingExperimentName) ==
             kSmartLockBrandingGroupName;
}

}  // namespace password_bubble_experiment

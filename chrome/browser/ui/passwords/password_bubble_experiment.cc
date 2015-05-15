// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_bubble_experiment.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/password_manager/password_manager_util.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"

namespace password_bubble_experiment {
namespace {

const char kBrandingExperimentName[] = "PasswordBranding";
const char kSmartLockBrandingGroupName[] = "SmartLockBranding";

} // namespace

void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason) {
  // TODO(vasilii): store the statistics.
}

bool IsSmartLockBrandingEnabled(Profile* profile) {
  const ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return password_manager_util::GetPasswordSyncState(sync_service) ==
             password_manager::SYNCING_NORMAL_ENCRYPTION &&
         base::FieldTrialList::FindFullName(kBrandingExperimentName) ==
             kSmartLockBrandingGroupName;
}

}  // namespace password_bubble_experiment

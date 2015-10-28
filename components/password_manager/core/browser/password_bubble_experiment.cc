// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_bubble_experiment {

const char kBrandingExperimentName[] = "PasswordBranding";
const char kSmartLockBrandingGroupName[] = "SmartLockBranding";
const char kSmartLockBrandingSavePromptOnlyGroupName[] =
    "SmartLockBrandingSavePromptOnly";

void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason) {
  // TODO(vasilii): store the statistics and consider merging with
  // password_manager_metrics_util.*.
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      password_manager::prefs::kWasSavePrompFirstRunExperienceShown, false);

  registry->RegisterBooleanPref(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
}

bool IsSmartLockUser(const sync_driver::SyncService* sync_service) {
  return password_manager_util::GetPasswordSyncState(sync_service) ==
         password_manager::SYNCING_NORMAL_ENCRYPTION;
}

SmartLockBranding GetSmartLockBrandingState(
    const sync_driver::SyncService* sync_service) {
  // Query the group first for correct UMA reporting.
  std::string group_name =
      base::FieldTrialList::FindFullName(kBrandingExperimentName);
  if (!IsSmartLockUser(sync_service))
    return SmartLockBranding::NONE;
  if (group_name == kSmartLockBrandingGroupName)
    return SmartLockBranding::FULL;
  if (group_name == kSmartLockBrandingSavePromptOnlyGroupName)
    return SmartLockBranding::SAVE_BUBBLE_ONLY;
  return SmartLockBranding::NONE;
}

bool IsSmartLockBrandingEnabled(const sync_driver::SyncService* sync_service) {
  return GetSmartLockBrandingState(sync_service) == SmartLockBranding::FULL;
}

bool ShouldShowSavePromptFirstRunExperience(
    const sync_driver::SyncService* sync_service,
    PrefService* prefs) {
  const bool was_first_run_experience_shown = prefs->GetBoolean(
      password_manager::prefs::kWasSavePrompFirstRunExperienceShown);
  return IsSmartLockBrandingEnabled(sync_service) &&
         !was_first_run_experience_shown;
}

void RecordSavePromptFirstRunExperienceWasShown(PrefService* prefs) {
  prefs->SetBoolean(
      password_manager::prefs::kWasSavePrompFirstRunExperienceShown, true);
}

bool ShouldShowAutoSignInPromptFirstRunExperience(PrefService* prefs) {
  return !prefs->GetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown);
}

void RecordAutoSignInPromptFirstRunExperienceWasShown(PrefService* prefs) {
  prefs->SetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, true);
}

}  // namespace password_bubble_experiment

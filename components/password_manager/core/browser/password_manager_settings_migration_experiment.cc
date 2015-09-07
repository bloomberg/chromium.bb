// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_settings_migration_experiment.h"

#include "base/metrics/field_trial.h"

bool IsSettingsMigrationActive() {
  const char kFieldTrialName[] = "PasswordManagerSettingsMigration";
  const char kEnabledGroupName[] = "PasswordManagerSettingsMigration.Enable";
  return base::FieldTrialList::FindFullName(kFieldTrialName) ==
         kEnabledGroupName;
}

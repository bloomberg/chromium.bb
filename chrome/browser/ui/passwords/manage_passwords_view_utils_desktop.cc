// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_view_utils_desktop.h"

#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_settings_migration_experiment.h"

int GetPasswordManagerSettingsStringId(
    const syncer::SyncService* sync_service) {
  int smart_lock_users_ids = IDS_OPTIONS_PASSWORD_MANAGER_SMART_LOCK_ENABLE;
  int non_smart_lock_users_ids = IDS_OPTIONS_PASSWORD_MANAGER_ENABLE;

  if (password_bubble_experiment::IsSmartLockUser(sync_service) &&
      password_manager::IsSettingsMigrationActive()) {
    return smart_lock_users_ids;
  }
  return non_smart_lock_users_ids;
}

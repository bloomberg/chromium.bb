// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_util.h"

#include "chrome/browser/sync/profile_sync_service.h"

namespace password_manager_util {

password_manager::PasswordSyncState GetPasswordSyncState(
    const ProfileSyncService* sync_service) {
  if (sync_service && sync_service->HasSyncSetupCompleted() &&
      sync_service->SyncActive() &&
      sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    return sync_service->IsUsingSecondaryPassphrase()
               ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
               : password_manager::SYNCING_NORMAL_ENCRYPTION;
  }
  return password_manager::NOT_SYNCING_PASSWORDS;
}

}  // namespace password_manager_util

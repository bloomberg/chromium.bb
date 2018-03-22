// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_utils.h"

#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service.h"

namespace syncer {

bool IsTabSyncEnabledAndUnencrypted(SyncService* sync_service,
                                    PrefService* pref_service) {
  // Check field trials and settings allow sending the URL on suggest requests.
  SyncPrefs sync_prefs(pref_service);
  return sync_service && sync_service->CanSyncStart() &&
         sync_prefs.GetPreferredDataTypes(UserTypes()).Has(PROXY_TABS) &&
         !sync_service->GetEncryptedDataTypes().Has(SESSIONS);
}

UploadState GetUploadToGoogleState(const SyncService* sync_service,
                                   ModelType type) {
  // Note: Before configuration is done, GetPreferredDataTypes returns
  // "everything" (i.e. the default setting). If a data type is missing there,
  // it must be because the user explicitly disabled it.
  if (!sync_service || !sync_service->CanSyncStart() ||
      sync_service->IsLocalSyncEnabled() ||
      !sync_service->GetPreferredDataTypes().Has(type) ||
      sync_service->GetAuthError().IsPersistentError() ||
      sync_service->IsUsingSecondaryPassphrase()) {
    return UploadState::NOT_ACTIVE;
  }
  if (!sync_service->IsSyncActive() || !sync_service->ConfigurationDone()) {
    return UploadState::INITIALIZING;
  }
  return UploadState::ACTIVE;
}

}  // namespace syncer

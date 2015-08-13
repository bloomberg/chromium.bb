// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/sync_service_utils.h"

#include "components/sync_driver/sync_prefs.h"
#include "components/sync_driver/sync_service.h"

namespace sync_driver {

bool IsTabSyncEnabledAndUnencrypted(SyncService* sync_service,
                                    PrefService* pref_service) {
  // Check field trials and settings allow sending the URL on suggest requests.
  sync_driver::SyncPrefs sync_prefs(pref_service);
  return sync_service && sync_service->CanSyncStart() &&
         sync_prefs.GetPreferredDataTypes(syncer::UserTypes())
             .Has(syncer::PROXY_TABS) &&
         !sync_service->GetEncryptedDataTypes().Has(syncer::SESSIONS);
}

}  // namespace sync_driver

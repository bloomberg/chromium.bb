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

}  // namespace syncer

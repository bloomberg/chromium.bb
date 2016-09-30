// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_UTILS_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_UTILS_H_

class PrefService;

namespace syncer {

class SyncService;

// Returns whether sync is enabled and tab sync is configured for syncing
// without encryption.
bool IsTabSyncEnabledAndUnencrypted(SyncService* sync_service,
                                    PrefService* pref_service);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_UTILS_H_

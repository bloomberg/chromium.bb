// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#define COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_

#include "components/sync/driver/profile_sync_service.h"

namespace browser_sync {

// TODO(crbug.com/896303): Have clients use syncer::ProfileSyncService directly
// and then get rid of this header.
using ProfileSyncService = syncer::ProfileSyncService;

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_

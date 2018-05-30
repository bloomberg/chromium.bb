// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_sync_bridge.h"

#include "components/sync/protocol/sync.pb.h"

namespace syncer {

ConsentSyncBridge::ConsentSyncBridge(
    std::unique_ptr<ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {}

}  // namespace syncer

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/syncable_service.h"

#include <utility>

namespace syncer {

SyncableService::SyncableService() {}

SyncableService::~SyncableService() {}

void SyncableService::WaitUntilReadyToSync(base::OnceClosure done) {
  std::move(done).Run();
}

}  // namespace syncer

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/sync_service_observer.h"

namespace sync_driver {

void SyncServiceObserver::OnSyncCycleCompleted() {
  OnStateChanged();
}

}  // namespace sync_driver

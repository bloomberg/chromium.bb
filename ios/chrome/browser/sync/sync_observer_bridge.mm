// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/sync_observer_bridge.h"

#include "base/logging.h"
#include "components/sync_driver/sync_service.h"

SyncObserverBridge::SyncObserverBridge(id<SyncObserverModelBridge> delegate,
                                       sync_driver::SyncService* sync_service)
    : delegate_(delegate), scoped_observer_(this) {
  DCHECK(delegate);
  if (sync_service)
    scoped_observer_.Add(sync_service);
}

SyncObserverBridge::~SyncObserverBridge() {
}

void SyncObserverBridge::OnStateChanged() {
  [delegate_ onSyncStateChanged];
}

void SyncObserverBridge::OnSyncConfigurationCompleted() {
  if ([delegate_ respondsToSelector:@selector(onSyncConfigurationCompleted:)])
    [delegate_ onSyncConfigurationCompleted];
}

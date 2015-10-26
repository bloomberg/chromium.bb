// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SYNC_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SYNC_SYNC_OBSERVER_BRIDGE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/ios/weak_nsobject.h"
#include "base/scoped_observer.h"
#include "components/sync_driver/sync_service_observer.h"

namespace sync_driver {
class SyncService;
}

@protocol SyncObserverModelBridge<NSObject>
- (void)onSyncStateChanged;
@optional
- (void)onSyncConfigurationCompleted;
@end

// C++ class to monitor profile sync status in Objective-C type.
class SyncObserverBridge : public sync_driver::SyncServiceObserver {
 public:
  // |service| must outlive the SyncObserverBridge.
  SyncObserverBridge(id<SyncObserverModelBridge> delegate,
                     sync_driver::SyncService* service);

  ~SyncObserverBridge() override;

 private:
   // sync_driver::SyncServiceObserver implementation:
   void OnStateChanged() override;
   void OnSyncConfigurationCompleted() override;

  base::WeakNSProtocol<id<SyncObserverModelBridge>> delegate_;
  ScopedObserver<sync_driver::SyncService, sync_driver::SyncServiceObserver>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(SyncObserverBridge);
};

#endif  // IOS_CHROME_BROWSER_SYNC_SYNC_OBSERVER_BRIDGE_H_

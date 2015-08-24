// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_BROWSING_DATA_PARTITION_IMPL_H_
#define IOS_WEB_BROWSING_DATA_PARTITION_IMPL_H_

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "ios/web/public/active_state_manager.h"
#include "ios/web/public/browsing_data_partition.h"

@class CRWBrowsingDataStore;

namespace web {

class BrowserState;

// Concrete subclass of web::BrowsingDataPartition. Observes
// ActiveStateManager::Observer methods to trigger stash/restore operations
// on the underlying CRWBrowsingDataStore.
class BrowsingDataPartitionImpl : public BrowsingDataPartition,
                                  public base::SupportsUserData::Data,
                                  public web::ActiveStateManager::Observer {
 public:
  explicit BrowsingDataPartitionImpl(BrowserState* browser_state);
  ~BrowsingDataPartitionImpl() override;

  // BrowsingDataPartition implementation.
  CRWBrowsingDataStore* GetBrowsingDataStore() override;

  // ActiveStateManager::Observer implementation.
  void OnActive() override;
  void OnInactive() override;
  void WillBeDestroyed() override;

 private:
  BrowserState* browser_state_;  // weak, owns this object.
  // The browsing data store backing this object.
  base::scoped_nsobject<CRWBrowsingDataStore> browsing_data_store_;
  // Weak pointer to the associated active state manager.
  ActiveStateManager* active_state_manager_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataPartitionImpl);
};

}  // namespace web

#endif  // IOS_WEB_BROWSING_DATA_PARTITION_IMPL_H_

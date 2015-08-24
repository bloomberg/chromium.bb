// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/browsing_data_partition_impl.h"

#include "base/logging.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/browsing_data_partition_client.h"
#import "ios/web/public/crw_browsing_data_store.h"
#include "ios/web/public/web_thread.h"

// A class that observes the |mode| changes in a CRWBrowsingDataStore using KVO.
// Maintains a count of all the CRWBrowsingDataStores whose mode is out of sync
// with their corresponding ActiveStateManager's active state.
@interface CRWBrowsingDataStoreModeObserver : NSObject

// The total count of the CRWBrowsingDataStores -- that are being observed --
// whose mode is out of sync with their associated ActiveStateManager's active
// state.
@property(nonatomic, assign) NSUInteger outOfSyncStoreCount;

// Adds |browsingDataStore| to the list of the CRWBrowsingDataStores that are
// being observed for KVO changes in the |mode| value. |browserState| cannot be
// null and the |browserState|'s associated CRWBrowsingDataStore must be
// |browsingDataStore|.
// The |browsingDataStore|'s mode must not already be |CHANGING|.
- (void)startObservingBrowsingDataStore:(CRWBrowsingDataStore*)browsingDataStore
                           browserState:(web::BrowserState*)browserState;

// Stops observing |browsingDataStore| for its |mode| change.
// The |browsingDataStore|'s mode must not be |CHANGING|.
- (void)stopObservingBrowsingDataStore:(CRWBrowsingDataStore*)browsingDataStore;
@end

@implementation CRWBrowsingDataStoreModeObserver

@synthesize outOfSyncStoreCount = _outOfSyncStoreCount;

- (void)startObservingBrowsingDataStore:(CRWBrowsingDataStore*)browsingDataStore
                           browserState:(web::BrowserState*)browserState {
  web::BrowsingDataPartition* browsing_data_partition =
      web::BrowserState::GetBrowsingDataPartition(browserState);
  DCHECK(browsing_data_partition);
  DCHECK_EQ(browsing_data_partition->GetBrowsingDataStore(), browsingDataStore);
  DCHECK_NE(web::CHANGING, browsingDataStore.mode);

  [browsingDataStore addObserver:self
                      forKeyPath:@"mode"
                         options:0
                         context:browserState];
}

- (void)stopObservingBrowsingDataStore:
        (CRWBrowsingDataStore*)browsingDataStore {
  DCHECK_NE(web::CHANGING, browsingDataStore.mode);

  [browsingDataStore removeObserver:self forKeyPath:@"mode"];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(CRWBrowsingDataStore*)browsingDataStore
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK([keyPath isEqual:@"mode"]);
  DCHECK([browsingDataStore isKindOfClass:[CRWBrowsingDataStore class]]);
  DCHECK(context);

  if (browsingDataStore.mode == web::CHANGING) {
    ++self.outOfSyncStoreCount;
    return;
  }
  web::BrowserState* browserState = static_cast<web::BrowserState*>(context);
  web::ActiveStateManager* activeStateManager =
      web::BrowserState::GetActiveStateManager(browserState);
  DCHECK(activeStateManager);
  bool activeState = activeStateManager->IsActive();
  // Check if the |browsingDataStore|'s associated ActiveStateManager's active
  // state is still out of sync.
  if (activeState && browsingDataStore.mode == web::INACTIVE) {
    [browsingDataStore makeActiveWithCompletionHandler:nil];
  } else if (!activeState && browsingDataStore.mode == web::ACTIVE) {
    [browsingDataStore makeInactiveWithCompletionHandler:nil];
  }

  DCHECK_GE(self.outOfSyncStoreCount, 1U);
  --self.outOfSyncStoreCount;
  web::BrowsingDataPartitionClient* client =
      web::GetBrowsingDataPartitionClient();
  if (client) {
    client->DidBecomeSynchronized();
  }
}

@end

namespace web {

namespace {
// The global observer that tracks the number of CRWBrowsingDataStores whose
// modes are out of sync with their associated ActiveStateManager's active
// state.
CRWBrowsingDataStoreModeObserver* g_browsing_data_store_mode_observer = nil;
}  // namespace

BrowsingDataPartitionImpl::BrowsingDataPartitionImpl(
    BrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  DCHECK(browser_state);

  active_state_manager_ = BrowserState::GetActiveStateManager(browser_state);
  DCHECK(active_state_manager_);
  active_state_manager_->AddObserver(this);
}

BrowsingDataPartitionImpl::~BrowsingDataPartitionImpl() {
  if (active_state_manager_) {
    active_state_manager_->RemoveObserver(this);
  }
  DCHECK_NE(CHANGING, [browsing_data_store_ mode]);
  [g_browsing_data_store_mode_observer
      stopObservingBrowsingDataStore:browsing_data_store_];
}

// static
bool BrowsingDataPartition::IsSynchronized() {
  return [g_browsing_data_store_mode_observer outOfSyncStoreCount] == 0U;
}

CRWBrowsingDataStore* BrowsingDataPartitionImpl::GetBrowsingDataStore() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);

  if (!browsing_data_store_) {
    browsing_data_store_.reset(
        [[CRWBrowsingDataStore alloc] initWithBrowserState:browser_state_]);
    if (!g_browsing_data_store_mode_observer) {
      g_browsing_data_store_mode_observer =
          [[CRWBrowsingDataStoreModeObserver alloc] init];
    }
    [g_browsing_data_store_mode_observer
        startObservingBrowsingDataStore:browsing_data_store_
                           browserState:browser_state_];
  }
  return browsing_data_store_;
}

void BrowsingDataPartitionImpl::OnActive() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);

  [GetBrowsingDataStore() makeActiveWithCompletionHandler:nil];
}

void BrowsingDataPartitionImpl::OnInactive() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);

  [GetBrowsingDataStore() makeInactiveWithCompletionHandler:nil];
}

void BrowsingDataPartitionImpl::WillBeDestroyed() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);

  active_state_manager_->RemoveObserver(this);
  active_state_manager_ = nullptr;
}

}  // namespace web

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/browsing_data_partition_impl.h"

#include "base/logging.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/crw_browsing_data_store.h"
#include "ios/web/public/web_thread.h"

namespace web {

// static
BrowsingDataPartition* BrowsingDataPartition::Create(
    BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  DCHECK(browser_state);

  return new BrowsingDataPartitionImpl(browser_state);
}

BrowsingDataPartitionImpl::BrowsingDataPartitionImpl(
    BrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  DCHECK(browser_state);

  active_state_manager_ = static_cast<ActiveStateManagerImpl*>(
      BrowserState::GetActiveStateManager(browser_state));
  DCHECK(active_state_manager_);
  active_state_manager_->AddObserver(this);
}

BrowsingDataPartitionImpl::~BrowsingDataPartitionImpl() {
  if (active_state_manager_) {
    active_state_manager_->RemoveObserver(this);
  }
}

CRWBrowsingDataStore* BrowsingDataPartitionImpl::GetBrowsingDataStore() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);

  if (!browsing_data_store_) {
    browsing_data_store_.reset(
        [[CRWBrowsingDataStore alloc] initWithBrowserState:browser_state_]);
  }
  return browsing_data_store_;
}

void BrowsingDataPartitionImpl::OnActive() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);

  // TODO(shreyasv): Drive restoring of browsing data from here, once that API
  // is ready. crbug.com/480654
}

void BrowsingDataPartitionImpl::OnInactive() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);

  // TODO(shreyasv): Drive stashing of browsing data from here, once that API
  // is ready. crbug.com/480654
}

void BrowsingDataPartitionImpl::WillBeDestroyed() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);

  active_state_manager_->RemoveObserver(this);
  active_state_manager_ = nullptr;
}

}  // namespace web

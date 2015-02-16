// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/merge_session_observer_bridge.h"

#include "base/logging.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "google_apis/gaia/google_service_auth_error.h"

MergeSessionObserverBridge::MergeSessionObserverBridge(
    id<MergeSessionObserverBridgeDelegate> delegate,
    AccountReconcilor* account_reconcilor)
    : delegate_(delegate), account_reconcilor_(account_reconcilor) {
  DCHECK(delegate);
  DCHECK(account_reconcilor);
  account_reconcilor_->AddMergeSessionObserver(this);
}

MergeSessionObserverBridge::~MergeSessionObserverBridge() {
  account_reconcilor_->RemoveMergeSessionObserver(this);
}

void MergeSessionObserverBridge::MergeSessionCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  [delegate_ onMergeSessionCompleted:account_id error:error];
}

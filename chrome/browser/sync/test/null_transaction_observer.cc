// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/null_transaction_observer.h"

#include "base/memory/weak_ptr.h"

namespace syncable {

browser_sync::WeakHandle<TransactionObserver> NullTransactionObserver() {
  return browser_sync::MakeWeakHandle(base::WeakPtr<TransactionObserver>());
}

}  // namespace syncable

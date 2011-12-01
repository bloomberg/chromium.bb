// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_
#pragma once

#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace syncable {

class TransactionObserver {
 public:
  virtual void OnTransactionWrite(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      const ModelTypeBitSet& models_with_changes) = 0;
 protected:
  virtual ~TransactionObserver() {}
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_

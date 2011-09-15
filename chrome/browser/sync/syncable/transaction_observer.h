// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_
#pragma once

#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace syncable {

class TransactionObserver {
 public:
  virtual void OnTransactionStart(
      const tracked_objects::Location& location,
      const WriterTag& writer) = 0;
  virtual void OnTransactionMutate(
      const tracked_objects::Location& location,
      const WriterTag& writer,
      const SharedEntryKernelMutationMap& mutations,
      const ModelTypeBitSet& models_with_changes) = 0;
  virtual void OnTransactionEnd(
      const tracked_objects::Location& location,
      const WriterTag& writer) = 0;
 protected:
  virtual ~TransactionObserver() {}
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_TRANSACTION_OBSERVER_H_

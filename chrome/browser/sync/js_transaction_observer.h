// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_TRANSACTION_OBSERVER_H_
#define CHROME_BROWSER_SYNC_JS_TRANSACTION_OBSERVER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/syncable/transaction_observer.h"

namespace browser_sync {

class JsEventRouter;

// Routes SyncManager events to a JsEventRouter.
class JsTransactionObserver : public syncable::TransactionObserver {
 public:
  // |parent_router| must be non-NULL and must outlive this object.
  explicit JsTransactionObserver(JsEventRouter* parent_router);

  virtual ~JsTransactionObserver();

  // syncable::TransactionObserver implementation.
  virtual void OnTransactionStart(
      const tracked_objects::Location& location,
      const syncable::WriterTag& writer) OVERRIDE;
  virtual void OnTransactionMutate(
      const tracked_objects::Location& location,
      const syncable::WriterTag& writer,
      const syncable::EntryKernelMutationSet& mutations,
      const syncable::ModelTypeBitSet& models_with_changes) OVERRIDE;
  virtual void OnTransactionEnd(
      const tracked_objects::Location& location,
      const syncable::WriterTag& writer) OVERRIDE;

 private:
  base::NonThreadSafe non_thread_safe_;
  JsEventRouter* parent_router_;

  DISALLOW_COPY_AND_ASSIGN(JsTransactionObserver);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_TRANSACTION_OBSERVER_H_

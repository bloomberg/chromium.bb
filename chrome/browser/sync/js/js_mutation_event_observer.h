// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_JS_MUTATION_EVENT_OBSERVER_H_
#define CHROME_BROWSER_SYNC_JS_JS_MUTATION_EVENT_OBSERVER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/internal_api/sync_manager.h"
#include "chrome/browser/sync/syncable/transaction_observer.h"
#include "chrome/browser/sync/util/weak_handle.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace browser_sync {

class JsEventDetails;
class JsEventHandler;

// Observes all change- and transaction-related events and routes a
// summarized version to a JsEventHandler.
class JsMutationEventObserver
    : public sync_api::SyncManager::ChangeObserver,
      public syncable::TransactionObserver {
 public:
  JsMutationEventObserver();

  virtual ~JsMutationEventObserver();

  base::WeakPtr<JsMutationEventObserver> AsWeakPtr();

  void InvalidateWeakPtrs();

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler);

  // sync_api::SyncManager::ChangeObserver implementation.
  virtual void OnChangesApplied(
      syncable::ModelType model_type,
      int64 write_transaction_id,
      const sync_api::ImmutableChangeRecordList& changes) OVERRIDE;
  virtual void OnChangesComplete(syncable::ModelType model_type) OVERRIDE;

  // syncable::TransactionObserver implementation.
  virtual void OnTransactionWrite(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      const syncable::ModelTypeBitSet& models_with_changes) OVERRIDE;

 private:
  base::NonThreadSafe non_thread_safe_;
  base::WeakPtrFactory<JsMutationEventObserver> weak_ptr_factory_;
  WeakHandle<JsEventHandler> event_handler_;

  void HandleJsEvent(
    const tracked_objects::Location& from_here,
    const std::string& name, const JsEventDetails& details);

  DISALLOW_COPY_AND_ASSIGN(JsMutationEventObserver);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_JS_MUTATION_EVENT_OBSERVER_H_

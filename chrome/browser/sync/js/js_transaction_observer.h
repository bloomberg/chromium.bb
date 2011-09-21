// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_JS_TRANSACTION_OBSERVER_H_
#define CHROME_BROWSER_SYNC_JS_JS_TRANSACTION_OBSERVER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/syncable/transaction_observer.h"
#include "chrome/browser/sync/util/weak_handle.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace browser_sync {

class JsEventDetails;
class JsEventHandler;

// Routes SyncManager events to a JsEventHandler.
class JsTransactionObserver : public syncable::TransactionObserver {
 public:
  JsTransactionObserver();

  virtual ~JsTransactionObserver();

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler);

  // syncable::TransactionObserver implementation.
  virtual void OnTransactionStart(
      const tracked_objects::Location& location,
      const syncable::WriterTag& writer) OVERRIDE;
  virtual void OnTransactionWrite(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      const syncable::ModelTypeBitSet& models_with_changes) OVERRIDE;
  virtual void OnTransactionEnd(
      const tracked_objects::Location& location,
      const syncable::WriterTag& writer) OVERRIDE;

 private:
  base::NonThreadSafe non_thread_safe_;
  WeakHandle<JsEventHandler> event_handler_;

  void HandleJsEvent(
    const tracked_objects::Location& from_here,
    const std::string& name, const JsEventDetails& details);

  DISALLOW_COPY_AND_ASSIGN(JsTransactionObserver);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_JS_TRANSACTION_OBSERVER_H_

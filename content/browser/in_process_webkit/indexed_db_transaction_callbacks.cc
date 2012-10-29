// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_transaction_callbacks.h"

#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "content/common/indexed_db/indexed_db_messages.h"

namespace content {

IndexedDBTransactionCallbacks::IndexedDBTransactionCallbacks(
    IndexedDBDispatcherHost* dispatcher_host,
    int thread_id,
    int transaction_id)
    : dispatcher_host_(dispatcher_host),
      thread_id_(thread_id),
      transaction_id_(transaction_id) {
}

IndexedDBTransactionCallbacks::~IndexedDBTransactionCallbacks() {
}

void IndexedDBTransactionCallbacks::onAbort(
    const WebKit::WebIDBDatabaseError& error) {
  dispatcher_host_->Send(
      new IndexedDBMsg_TransactionCallbacksAbort(
          thread_id_, transaction_id_, error.code(), error.message()));
}

void IndexedDBTransactionCallbacks::onComplete() {
  dispatcher_host_->TransactionComplete(transaction_id_);
  dispatcher_host_->Send(
      new IndexedDBMsg_TransactionCallbacksComplete(thread_id_,
                                                    transaction_id_));
}

}  // namespace content

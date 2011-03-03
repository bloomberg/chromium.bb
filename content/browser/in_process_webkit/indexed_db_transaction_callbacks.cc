// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_transaction_callbacks.h"

#include "chrome/common/indexed_db_messages.h"
#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"

IndexedDBTransactionCallbacks::IndexedDBTransactionCallbacks(
    IndexedDBDispatcherHost* dispatcher_host,
    int transaction_id)
    : dispatcher_host_(dispatcher_host),
      transaction_id_(transaction_id) {
}

IndexedDBTransactionCallbacks::~IndexedDBTransactionCallbacks() {
}

void IndexedDBTransactionCallbacks::onAbort() {
  dispatcher_host_->Send(
      new IndexedDBMsg_TransactionCallbacksAbort(transaction_id_));
}

void IndexedDBTransactionCallbacks::onComplete() {
  dispatcher_host_->Send(
      new IndexedDBMsg_TransactionCallbacksComplete(transaction_id_));
}

void IndexedDBTransactionCallbacks::onTimeout() {
  dispatcher_host_->Send(
      new IndexedDBMsg_TransactionCallbacksTimeout(transaction_id_));
}

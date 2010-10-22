// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/indexed_db_callbacks.h"

IndexedDBCallbacksBase::IndexedDBCallbacksBase(
    IndexedDBDispatcherHost* dispatcher_host,
    int32 response_id)
    : dispatcher_host_(dispatcher_host),
      response_id_(response_id) {
}

IndexedDBCallbacksBase::~IndexedDBCallbacksBase() {}

IndexedDBTransactionCallbacks::IndexedDBTransactionCallbacks(
    IndexedDBDispatcherHost* dispatcher_host,
    int transaction_id)
    : dispatcher_host_(dispatcher_host),
      transaction_id_(transaction_id) {
}

IndexedDBTransactionCallbacks::~IndexedDBTransactionCallbacks() {}

void IndexedDBCallbacksBase::onError(const WebKit::WebIDBDatabaseError& error) {
  dispatcher_host_->Send(new ViewMsg_IDBCallbacksError(
      response_id_, error.code(), error.message()));
}

void IndexedDBTransactionCallbacks::onAbort() {
  dispatcher_host_->Send(
      new ViewMsg_IDBTransactionCallbacksAbort(transaction_id_));
}

void IndexedDBTransactionCallbacks::onComplete() {
  dispatcher_host_->Send(
      new ViewMsg_IDBTransactionCallbacksComplete(transaction_id_));
}

void IndexedDBTransactionCallbacks::onTimeout() {
  dispatcher_host_->Send(
      new ViewMsg_IDBTransactionCallbacksTimeout(transaction_id_));
}

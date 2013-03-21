// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_database_callbacks.h"

#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "content/common/indexed_db/indexed_db_messages.h"

namespace content {

IndexedDBDatabaseCallbacks::IndexedDBDatabaseCallbacks(
    IndexedDBDispatcherHost* dispatcher_host,
    int ipc_thread_id,
    int ipc_database_id)
    : dispatcher_host_(dispatcher_host),
      ipc_thread_id_(ipc_thread_id),
      ipc_database_id_(ipc_database_id) {
}

IndexedDBDatabaseCallbacks::~IndexedDBDatabaseCallbacks() {
}

void IndexedDBDatabaseCallbacks::onForcedClose() {
  dispatcher_host_->Send(
      new IndexedDBMsg_DatabaseCallbacksForcedClose(ipc_thread_id_,
                                                    ipc_database_id_));
}

void IndexedDBDatabaseCallbacks::onVersionChange(long long old_version,
                                                 long long new_version) {
  dispatcher_host_->Send(
      new IndexedDBMsg_DatabaseCallbacksIntVersionChange(ipc_thread_id_,
                                                         ipc_database_id_,
                                                         old_version,
                                                         new_version));
}

void IndexedDBDatabaseCallbacks::onAbort(
    long long host_transaction_id,
    const WebKit::WebIDBDatabaseError& error) {
    dispatcher_host_->FinishTransaction(host_transaction_id, false);
    dispatcher_host_->Send(
        new IndexedDBMsg_DatabaseCallbacksAbort(
            ipc_thread_id_, ipc_database_id_,
            dispatcher_host_->RendererTransactionId(host_transaction_id),
            error.code(), error.message()));
}

void IndexedDBDatabaseCallbacks::onComplete(long long host_transaction_id) {
    dispatcher_host_->FinishTransaction(host_transaction_id, true);
    dispatcher_host_->Send(
        new IndexedDBMsg_DatabaseCallbacksComplete(
            ipc_thread_id_, ipc_database_id_,
            dispatcher_host_->RendererTransactionId(host_transaction_id)));
}

}  // namespace content

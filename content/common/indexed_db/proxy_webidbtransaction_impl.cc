// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/proxy_webidbtransaction_impl.h"

#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/child_thread.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransactionCallbacks.h"

using WebKit::WebIDBObjectStore;
using WebKit::WebIDBTransactionCallbacks;
using WebKit::WebString;

namespace content {

RendererWebIDBTransactionImpl::RendererWebIDBTransactionImpl(
    int32 ipc_transaction_id)
    : ipc_transaction_id_(ipc_transaction_id) {
}

RendererWebIDBTransactionImpl::~RendererWebIDBTransactionImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object wich owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_TransactionDestroyed(
      ipc_transaction_id_));
}

void RendererWebIDBTransactionImpl::commit() {
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_TransactionCommit(
      ipc_transaction_id_));
}

void RendererWebIDBTransactionImpl::abort() {
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_TransactionAbort(
      ipc_transaction_id_));
}

void RendererWebIDBTransactionImpl::setCallbacks(
    WebIDBTransactionCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RegisterWebIDBTransactionCallbacks(callbacks,
                                                 ipc_transaction_id_);
}

}  // namespace content

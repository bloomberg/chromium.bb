// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webidbtransaction_impl.h"

#include "content/common/indexed_db_messages.h"
#include "content/renderer/indexed_db_dispatcher.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webidbobjectstore_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransactionCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

using WebKit::WebIDBObjectStore;
using WebKit::WebIDBTransactionCallbacks;
using WebKit::WebString;

RendererWebIDBTransactionImpl::RendererWebIDBTransactionImpl(
    int32 idb_transaction_id)
    : idb_transaction_id_(idb_transaction_id) {
}

RendererWebIDBTransactionImpl::~RendererWebIDBTransactionImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object wich owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  ChildThread::current()->Send(new IndexedDBHostMsg_TransactionDestroyed(
      idb_transaction_id_));
}

int RendererWebIDBTransactionImpl::mode() const
{
  int mode;
  ChildThread::current()->Send(new IndexedDBHostMsg_TransactionMode(
      idb_transaction_id_, &mode));
  return mode;
}

WebIDBObjectStore* RendererWebIDBTransactionImpl::objectStore(
    const WebString& name,
    WebKit::WebExceptionCode& ec)
{
  int object_store_id;
  ChildThread::current()->Send(
      new IndexedDBHostMsg_TransactionObjectStore(
          idb_transaction_id_, name, &object_store_id, &ec));
  if (!object_store_id)
    return NULL;
  return new RendererWebIDBObjectStoreImpl(object_store_id);
}

void RendererWebIDBTransactionImpl::abort()
{
  ChildThread::current()->Send(new IndexedDBHostMsg_TransactionAbort(
      idb_transaction_id_));
}

void RendererWebIDBTransactionImpl::didCompleteTaskEvents()
{
  ChildThread::current()->Send(
      new IndexedDBHostMsg_TransactionDidCompleteTaskEvents(
          idb_transaction_id_));
}

void RendererWebIDBTransactionImpl::setCallbacks(
    WebIDBTransactionCallbacks* callbacks)
{
  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RegisterWebIDBTransactionCallbacks(callbacks,
                                                 idb_transaction_id_);
}

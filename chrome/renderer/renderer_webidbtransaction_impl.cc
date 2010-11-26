// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webidbtransaction_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/indexed_db_dispatcher.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/renderer_webidbobjectstore_impl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBObjectStore.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBTransactionCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebIDBObjectStore;
using WebKit::WebIDBTransactionCallbacks;
using WebKit::WebString;

RendererWebIDBTransactionImpl::RendererWebIDBTransactionImpl(
    int32 idb_transaction_id)
    : idb_transaction_id_(idb_transaction_id) {
}

RendererWebIDBTransactionImpl::~RendererWebIDBTransactionImpl() {
  RenderThread::current()->Send(new ViewHostMsg_IDBTransactionDestroyed(
      idb_transaction_id_));
}

int RendererWebIDBTransactionImpl::mode() const
{
  int mode;
  RenderThread::current()->Send(new ViewHostMsg_IDBTransactionMode(
      idb_transaction_id_, &mode));
  return mode;
}

WebIDBObjectStore* RendererWebIDBTransactionImpl::objectStore(
    const WebString& name,
    WebKit::WebExceptionCode& ec)
{
  int object_store_id;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBTransactionObjectStore(
          idb_transaction_id_, name, &object_store_id, &ec));
  if (!object_store_id)
    return NULL;
  return new RendererWebIDBObjectStoreImpl(object_store_id);
}

void RendererWebIDBTransactionImpl::abort()
{
  RenderThread::current()->Send(new ViewHostMsg_IDBTransactionAbort(
      idb_transaction_id_));
}

void RendererWebIDBTransactionImpl::didCompleteTaskEvents()
{
  RenderThread::current()->Send(
      new ViewHostMsg_IDBTransactionDidCompleteTaskEvents(
          idb_transaction_id_));
}

void RendererWebIDBTransactionImpl::setCallbacks(
    WebIDBTransactionCallbacks* callbacks)
{
  IndexedDBDispatcher* dispatcher =
      RenderThread::current()->indexed_db_dispatcher();
  dispatcher->RegisterWebIDBTransactionCallbacks(callbacks,
                                                 idb_transaction_id_);
}

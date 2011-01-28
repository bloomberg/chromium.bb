// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indexed_db_dispatcher.h"

#include "chrome/common/indexed_db_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/renderer_webidbcursor_impl.h"
#include "chrome/renderer/renderer_webidbdatabase_impl.h"
#include "chrome/renderer/renderer_webidbindex_impl.h"
#include "chrome/renderer/renderer_webidbobjectstore_impl.h"
#include "chrome/renderer/renderer_webidbtransaction_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

using WebKit::WebExceptionCode;
using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseError;
using WebKit::WebIDBTransaction;
using WebKit::WebIDBTransactionCallbacks;

IndexedDBDispatcher::IndexedDBDispatcher() {
}

IndexedDBDispatcher::~IndexedDBDispatcher() {
}

bool IndexedDBDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IndexedDBDispatcher, msg)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBCursor,
                        OnSuccessOpenCursor)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBDatabase,
                        OnSuccessIDBDatabase)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBIndex,
                        OnSuccessIDBIndex)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIndexedDBKey,
                        OnSuccessIndexedDBKey)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBObjectStore,
                        OnSuccessIDBObjectStore)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBTransaction,
                        OnSuccessIDBTransaction)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessSerializedScriptValue,
                        OnSuccessSerializedScriptValue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksError, OnError)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_TransactionCallbacksAbort, OnAbort)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_TransactionCallbacksComplete, OnComplete)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_TransactionCallbacksTimeout, OnTimeout)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcher::RequestIDBCursorUpdate(
    const SerializedScriptValue& value,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new IndexedDBHostMsg_CursorUpdate(idb_cursor_id, response_id, value, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBCursorContinue(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new IndexedDBHostMsg_CursorContinue(idb_cursor_id, response_id, key, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBCursorDelete(
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new IndexedDBHostMsg_CursorDelete(idb_cursor_id, response_id, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBFactoryOpen(
    const string16& name,
    WebIDBCallbacks* callbacks_ptr,
    const string16& origin,
    WebFrame* web_frame,
    uint64 maximum_size) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (!web_frame)
    return; // We must be shutting down.
  RenderView* render_view = RenderView::FromWebView(web_frame->view());
  if (!render_view)
    return; // We must be shutting down.

  IndexedDBHostMsg_FactoryOpen_Params params;
  params.routing_id = render_view->routing_id();
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.origin = origin;
  params.name = name;
  params.maximum_size = maximum_size;
  RenderThread::current()->Send(new IndexedDBHostMsg_FactoryOpen(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseSetVersion(
    const string16& version,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_database_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new IndexedDBHostMsg_DatabaseSetVersion(idb_database_id, response_id,
                                            version, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBIndexOpenObjectCursor(
    const WebIDBKeyRange& idb_key_range,
    unsigned short direction,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBHostMsg_IndexOpenCursor_Params params;
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.lower_key.Set(idb_key_range.lower());
  params.upper_key.Set(idb_key_range.upper());
  params.lower_open = idb_key_range.lowerOpen();
  params.upper_open = idb_key_range.upperOpen();
  params.direction = direction;
  params.idb_index_id = idb_index_id;
  params.transaction_id = TransactionId(transaction);
  RenderThread::current()->Send(
      new IndexedDBHostMsg_IndexOpenObjectCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RequestIDBIndexOpenKeyCursor(
    const WebIDBKeyRange& idb_key_range,
    unsigned short direction,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBHostMsg_IndexOpenCursor_Params params;
  params.response_id = pending_callbacks_.Add(callbacks.release());
  // TODO(jorlow): We really should just create a Chromium abstraction for
  //               KeyRange rather than doing it ad-hoc like this.
  params.lower_key.Set(idb_key_range.lower());
  params.upper_key.Set(idb_key_range.upper());
  params.lower_open = idb_key_range.lowerOpen();
  params.upper_open = idb_key_range.upperOpen();
  params.direction = direction;
  params.idb_index_id = idb_index_id;
  params.transaction_id = TransactionId(transaction);
  RenderThread::current()->Send(
      new IndexedDBHostMsg_IndexOpenKeyCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RequestIDBIndexGetObject(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new IndexedDBHostMsg_IndexGetObject(
          idb_index_id, response_id, key,
          TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBIndexGetKey(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new IndexedDBHostMsg_IndexGetKey(
          idb_index_id, response_id, key,
          TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreGet(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new IndexedDBHostMsg_ObjectStoreGet(
          idb_object_store_id, response_id,
          key, TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStorePut(
    const SerializedScriptValue& value,
    const IndexedDBKey& key,
    bool add_only,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBHostMsg_ObjectStorePut_Params params;
  params.idb_object_store_id = idb_object_store_id;
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.serialized_value = value;
  params.key = key;
  params.add_only = add_only;
  params.transaction_id = TransactionId(transaction);
  RenderThread::current()->Send(new IndexedDBHostMsg_ObjectStorePut(
      params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreDelete(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new IndexedDBHostMsg_ObjectStoreDelete(
          idb_object_store_id, response_id,
          key, TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreOpenCursor(
    const WebIDBKeyRange& idb_key_range,
    unsigned short direction,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  IndexedDBHostMsg_ObjectStoreOpenCursor_Params params;
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.lower_key.Set(idb_key_range.lower());
  params.upper_key.Set(idb_key_range.upper());
  params.lower_open = idb_key_range.lowerOpen();
  params.upper_open = idb_key_range.upperOpen();
  params.direction = direction;
  params.idb_object_store_id = idb_object_store_id;
  params.transaction_id = TransactionId(transaction);
  RenderThread::current()->Send(
      new IndexedDBHostMsg_ObjectStoreOpenCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RegisterWebIDBTransactionCallbacks(
    WebIDBTransactionCallbacks* callbacks,
    int32 id) {
  pending_transaction_callbacks_.AddWithID(callbacks, id);
}

int32 IndexedDBDispatcher::TransactionId(
    const WebIDBTransaction& transaction) {
  const RendererWebIDBTransactionImpl* impl =
      static_cast<const RendererWebIDBTransactionImpl*>(&transaction);
  return impl->id();
}

void IndexedDBDispatcher::OnSuccessIDBDatabase(int32 response_id,
                                               int32 object_id) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBDatabaseImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIndexedDBKey(int32 response_id,
                                                const IndexedDBKey& key) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(key);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIDBObjectStore(int32 response_id,
                                                  int32 object_id) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBObjectStoreImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIDBTransaction(int32 response_id,
                                                  int32 object_id) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBTransactionImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIDBIndex(int32 response_id,
                                            int32 object_id) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBIndexImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessSerializedScriptValue(
    int32 response_id, const SerializedScriptValue& value) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(value);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessOpenCursor(int32 repsonse_id,
                                              int32 object_id) {
  WebIDBCallbacks* callbacks =
      pending_callbacks_.Lookup(repsonse_id);
  callbacks->onSuccess(new RendererWebIDBCursorImpl(object_id));
  pending_callbacks_.Remove(repsonse_id);
}

void IndexedDBDispatcher::OnError(int32 response_id, int code,
                                  const string16& message) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onError(WebIDBDatabaseError(code, message));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnAbort(int32 transaction_id) {
  WebIDBTransactionCallbacks* callbacks =
      pending_transaction_callbacks_.Lookup(transaction_id);
  callbacks->onAbort();
  pending_transaction_callbacks_.Remove(transaction_id);
}

void IndexedDBDispatcher::OnComplete(int32 transaction_id) {
  WebIDBTransactionCallbacks* callbacks =
      pending_transaction_callbacks_.Lookup(transaction_id);
  callbacks->onComplete();
  pending_transaction_callbacks_.Remove(transaction_id);
}

void IndexedDBDispatcher::OnTimeout(int32 transaction_id) {
  WebIDBTransactionCallbacks* callbacks =
      pending_transaction_callbacks_.Lookup(transaction_id);
  callbacks->onTimeout();
  pending_transaction_callbacks_.Remove(transaction_id);
}

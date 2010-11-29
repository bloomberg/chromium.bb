// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indexed_db_dispatcher.h"

#include "chrome/common/indexed_db_key.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/serialized_script_value.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/renderer_webidbcursor_impl.h"
#include "chrome/renderer/renderer_webidbdatabase_impl.h"
#include "chrome/renderer/renderer_webidbindex_impl.h"
#include "chrome/renderer/renderer_webidbobjectstore_impl.h"
#include "chrome/renderer/renderer_webidbtransaction_impl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBKeyRange.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

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
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessNull,
                        OnSuccessNull)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIDBCursor,
                        OnSuccessOpenCursor)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIDBDatabase,
                        OnSuccessIDBDatabase)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIDBIndex,
                        OnSuccessIDBIndex)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIndexedDBKey,
                        OnSuccessIndexedDBKey)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIDBObjectStore,
                        OnSuccessIDBObjectStore)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIDBTransaction,
                        OnSuccessIDBTransaction)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessSerializedScriptValue,
                        OnSuccessSerializedScriptValue)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksError,
                        OnError)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBTransactionCallbacksAbort,
                        OnAbort)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBTransactionCallbacksComplete,
                        OnComplete)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBTransactionCallbacksTimeout,
                        OnTimeout)
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
      new ViewHostMsg_IDBCursorUpdate(idb_cursor_id, response_id, value, ec));
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
      new ViewHostMsg_IDBCursorContinue(idb_cursor_id, response_id, key, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBCursorRemove(
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new ViewHostMsg_IDBCursorRemove(idb_cursor_id, response_id, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBFactoryOpen(
    const string16& name,
    const string16& description,
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

  ViewHostMsg_IDBFactoryOpen_Params params;
  params.routing_id_ = render_view->routing_id();
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.origin_ = origin;
  params.name_ = name;
  params.description_ = description;
  params.maximum_size_ = maximum_size;
  RenderThread::current()->Send(new ViewHostMsg_IDBFactoryOpen(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseSetVersion(
    const string16& version,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_database_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseSetVersion(idb_database_id, response_id,
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
  ViewHostMsg_IDBIndexOpenCursor_Params params;
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.lower_key_.Set(idb_key_range.lower());
  params.upper_key_.Set(idb_key_range.upper());
  params.lower_open_ = idb_key_range.lowerOpen();
  params.upper_open_ = idb_key_range.upperOpen();
  params.direction_ = direction;
  params.idb_index_id_ = idb_index_id;
  params.transaction_id_ = TransactionId(transaction);
  RenderThread::current()->Send(
      new ViewHostMsg_IDBIndexOpenObjectCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id_);
}

void IndexedDBDispatcher::RequestIDBIndexOpenKeyCursor(
    const WebIDBKeyRange& idb_key_range,
    unsigned short direction,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_index_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  ViewHostMsg_IDBIndexOpenCursor_Params params;
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  // TODO(jorlow): We really should just create a Chromium abstraction for
  //               KeyRange rather than doing it ad-hoc like this.
  params.lower_key_.Set(idb_key_range.lower());
  params.upper_key_.Set(idb_key_range.upper());
  params.lower_open_ = idb_key_range.lowerOpen();
  params.upper_open_ = idb_key_range.upperOpen();
  params.direction_ = direction;
  params.idb_index_id_ = idb_index_id;
  params.transaction_id_ = TransactionId(transaction);
  RenderThread::current()->Send(
      new ViewHostMsg_IDBIndexOpenKeyCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id_);
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
      new ViewHostMsg_IDBIndexGetObject(
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
      new ViewHostMsg_IDBIndexGetKey(
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
      new ViewHostMsg_IDBObjectStoreGet(
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
  ViewHostMsg_IDBObjectStorePut_Params params;
  params.idb_object_store_id_ = idb_object_store_id;
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.serialized_value_ = value;
  params.key_ = key;
  params.add_only_ = add_only;
  params.transaction_id_ = TransactionId(transaction);
  RenderThread::current()->Send(new ViewHostMsg_IDBObjectStorePut(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id_);
}

void IndexedDBDispatcher::RequestIDBObjectStoreRemove(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreRemove(
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
  ViewHostMsg_IDBObjectStoreOpenCursor_Params params;
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.lower_key_.Set(idb_key_range.lower());
  params.upper_key_.Set(idb_key_range.upper());
  params.lower_open_ = idb_key_range.lowerOpen();
  params.upper_open_ = idb_key_range.upperOpen();
  params.direction_ = direction;
  params.idb_object_store_id_ = idb_object_store_id;
  params.transaction_id_ = TransactionId(transaction);
  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreOpenCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id_);
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

void IndexedDBDispatcher::OnSuccessNull(int32 response_id) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess();
  pending_callbacks_.Remove(response_id);
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

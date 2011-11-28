// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/indexed_db_dispatcher.h"

#include "content/common/indexed_db_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_webidbcursor_impl.h"
#include "content/renderer/renderer_webidbdatabase_impl.h"
#include "content/renderer/renderer_webidbindex_impl.h"
#include "content/renderer/renderer_webidbobjectstore_impl.h"
#include "content/renderer/renderer_webidbtransaction_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

using WebKit::WebDOMStringList;
using WebKit::WebExceptionCode;
using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseCallbacks;
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
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessCursorContinue,
                        OnSuccessCursorContinue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBDatabase,
                        OnSuccessIDBDatabase)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIndexedDBKey,
                        OnSuccessIndexedDBKey)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessIDBTransaction,
                        OnSuccessIDBTransaction)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessStringList,
                        OnSuccessStringList)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksSuccessSerializedScriptValue,
                        OnSuccessSerializedScriptValue)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksError, OnError)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_CallbacksBlocked, OnBlocked)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_TransactionCallbacksAbort, OnAbort)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_TransactionCallbacksComplete, OnComplete)
    IPC_MESSAGE_HANDLER(IndexedDBMsg_DatabaseCallbacksVersionChange,
                        OnVersionChange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcher::RequestIDBCursorUpdate(
    const content::SerializedScriptValue& value,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThreadImpl::current()->Send(
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
  RenderThreadImpl::current()->Send(
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
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_CursorDelete(idb_cursor_id, response_id, ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBFactoryOpen(
    const string16& name,
    WebIDBCallbacks* callbacks_ptr,
    const string16& origin,
    WebFrame* web_frame) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (!web_frame)
    return; // We must be shutting down.
  RenderViewImpl* render_view = RenderViewImpl::FromWebView(web_frame->view());
  if (!render_view)
    return; // We must be shutting down.

  IndexedDBHostMsg_FactoryOpen_Params params;
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.origin = origin;
  params.name = name;
  RenderThreadImpl::current()->Send(new IndexedDBHostMsg_FactoryOpen(params));
}

void IndexedDBDispatcher::RequestIDBFactoryGetDatabaseNames(
    WebIDBCallbacks* callbacks_ptr,
    const string16& origin,
    WebFrame* web_frame) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (!web_frame)
    return; // We must be shutting down.
  RenderViewImpl* render_view = RenderViewImpl::FromWebView(web_frame->view());
  if (!render_view)
    return; // We must be shutting down.

  IndexedDBHostMsg_FactoryGetDatabaseNames_Params params;
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.origin = origin;
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_FactoryGetDatabaseNames(params));
}

void IndexedDBDispatcher::RequestIDBFactoryDeleteDatabase(
    const string16& name,
    WebIDBCallbacks* callbacks_ptr,
    const string16& origin,
    WebFrame* web_frame) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (!web_frame)
    return; // We must be shutting down.
  RenderViewImpl* render_view = RenderViewImpl::FromWebView(web_frame->view());
  if (!render_view)
    return; // We must be shutting down.

  IndexedDBHostMsg_FactoryDeleteDatabase_Params params;
  params.response_id = pending_callbacks_.Add(callbacks.release());
  params.origin = origin;
  params.name = name;
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_FactoryDeleteDatabase(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseClose(int32 idb_database_id) {
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_DatabaseClose(idb_database_id));
  pending_database_callbacks_.Remove(idb_database_id);
}

  void IndexedDBDispatcher::RequestIDBDatabaseOpen(
      WebIDBDatabaseCallbacks* callbacks_ptr,
      int32 idb_database_id) {
  scoped_ptr<WebIDBDatabaseCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_database_callbacks_.Add(callbacks.release());
  RenderThreadImpl::current()->Send(new IndexedDBHostMsg_DatabaseOpen(
      response_id, idb_database_id));
}

void IndexedDBDispatcher::RequestIDBDatabaseSetVersion(
    const string16& version,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_database_id,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThreadImpl::current()->Send(
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
  RenderThreadImpl::current()->Send(
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
  RenderThreadImpl::current()->Send(
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
  RenderThreadImpl::current()->Send(
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
  RenderThreadImpl::current()->Send(
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
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_ObjectStoreGet(
          idb_object_store_id, response_id,
          key, TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStorePut(
    const content::SerializedScriptValue& value,
    const IndexedDBKey& key,
    WebKit::WebIDBObjectStore::PutMode put_mode,
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
  params.put_mode = put_mode;
  params.transaction_id = TransactionId(transaction);
  RenderThreadImpl::current()->Send(new IndexedDBHostMsg_ObjectStorePut(
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
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_ObjectStoreDelete(
          idb_object_store_id, response_id,
          key, TransactionId(transaction), ec));
  if (*ec)
    pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::RequestIDBObjectStoreClear(
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode* ec) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  int32 response_id = pending_callbacks_.Add(callbacks.release());
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_ObjectStoreClear(
          idb_object_store_id, response_id,
          TransactionId(transaction), ec));
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
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_ObjectStoreOpenCursor(params, ec));
  if (*ec)
    pending_callbacks_.Remove(params.response_id);
}

void IndexedDBDispatcher::RegisterWebIDBTransactionCallbacks(
    WebIDBTransactionCallbacks* callbacks,
    int32 id) {
  pending_transaction_callbacks_.AddWithID(callbacks, id);
}

void IndexedDBDispatcher::CursorDestroyed(int32 cursor_id) {
  cursors_.erase(cursor_id);
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
  if (!callbacks)
    return;
  callbacks->onSuccess(new RendererWebIDBDatabaseImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIndexedDBKey(int32 response_id,
                                                const IndexedDBKey& key) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(key);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIDBTransaction(int32 response_id,
                                                  int32 object_id) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(new RendererWebIDBTransactionImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessStringList(
    int32 response_id, const std::vector<string16>& value) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  WebDOMStringList string_list;
  for (std::vector<string16>::const_iterator it = value.begin();
       it != value.end(); ++it)
      string_list.append(*it);
  callbacks->onSuccess(string_list);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessSerializedScriptValue(
    int32 response_id, const content::SerializedScriptValue& value) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onSuccess(value);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessOpenCursor(int32 repsonse_id,
    int32 object_id, const IndexedDBKey& key, const IndexedDBKey& primary_key,
    const content::SerializedScriptValue& value) {
  WebIDBCallbacks* callbacks =
      pending_callbacks_.Lookup(repsonse_id);
  if (!callbacks)
    return;

  RendererWebIDBCursorImpl* cursor = new RendererWebIDBCursorImpl(object_id);
  cursors_[object_id] = cursor;
  cursor->SetKeyAndValue(key, primary_key, value);
  callbacks->onSuccess(cursor);

  pending_callbacks_.Remove(repsonse_id);
}

void IndexedDBDispatcher::OnSuccessCursorContinue(
    int32 response_id,
    int32 cursor_id,
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    const content::SerializedScriptValue& value) {
  RendererWebIDBCursorImpl* cursor = cursors_[cursor_id];
  DCHECK(cursor);
  cursor->SetKeyAndValue(key, primary_key, value);

  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onSuccessWithContinuation();

  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnBlocked(int32 response_id) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onBlocked();
}

void IndexedDBDispatcher::OnError(int32 response_id, int code,
                                  const string16& message) {
  WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  if (!callbacks)
    return;
  callbacks->onError(WebIDBDatabaseError(code, message));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnAbort(int32 transaction_id) {
  WebIDBTransactionCallbacks* callbacks =
      pending_transaction_callbacks_.Lookup(transaction_id);
  if (!callbacks)
    return;
  callbacks->onAbort();
  pending_transaction_callbacks_.Remove(transaction_id);
}

void IndexedDBDispatcher::OnComplete(int32 transaction_id) {
  WebIDBTransactionCallbacks* callbacks =
      pending_transaction_callbacks_.Lookup(transaction_id);
  if (!callbacks)
    return;
  callbacks->onComplete();
  pending_transaction_callbacks_.Remove(transaction_id);
}

void IndexedDBDispatcher::OnVersionChange(int32 database_id,
                                          const string16& newVersion) {
  WebIDBDatabaseCallbacks* callbacks =
      pending_database_callbacks_.Lookup(database_id);
  // callbacks would be NULL if a versionchange event is received after close
  // has been called.
  if (!callbacks)
    return;
  callbacks->onVersionChange(newVersion);
}

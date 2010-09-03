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
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBKeyRange.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseError;

IndexedDBDispatcher::IndexedDBDispatcher() {
}

IndexedDBDispatcher::~IndexedDBDispatcher() {
}

bool IndexedDBDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IndexedDBDispatcher, msg)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessNull,
                        OnSuccessNull)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIDBDatabase,
                        OnSuccessIDBDatabase)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIndexedDBKey,
                        OnSuccessIndexedDBKey)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIDBObjectStore,
                        OnSuccessIDBObjectStore)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessIDBIndex,
                        OnSuccessIDBIndex)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbackSuccessOpenCursor,
                        OnSuccessOpenCursor)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksSuccessSerializedScriptValue,
                        OnSuccessSerializedScriptValue)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbacksError,
                        OnError)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBTransactionCallbacksAbort,
                        OnAbort)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcher::RequestIDBCursorUpdate(
    const SerializedScriptValue& value,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  RenderThread::current()->Send(
      new ViewHostMsg_IDBCursorUpdate(
          idb_cursor_id, pending_callbacks_.Add(callbacks.release()), value));
}

void IndexedDBDispatcher::RequestIDBCursorContinue(
    const IndexedDBKey& key,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  RenderThread::current()->Send(
      new ViewHostMsg_IDBCursorContinue(
          idb_cursor_id, pending_callbacks_.Add(callbacks.release()), key));
}

void IndexedDBDispatcher::RequestIDBCursorRemove(
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_cursor_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  RenderThread::current()->Send(
      new ViewHostMsg_IDBCursorRemove(
          idb_cursor_id, pending_callbacks_.Add(callbacks.release())));
}

void IndexedDBDispatcher::RequestIDBFactoryOpen(
    const string16& name, const string16& description,
    WebIDBCallbacks* callbacks_ptr, const string16& origin,
    WebFrame* web_frame) {
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
  RenderThread::current()->Send(new ViewHostMsg_IDBFactoryOpen(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseCreateObjectStore(
    const string16& name, const NullableString16& key_path, bool auto_increment,
    WebIDBCallbacks* callbacks_ptr, int32 idb_database_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  ViewHostMsg_IDBDatabaseCreateObjectStore_Params params;
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.name_ = name;
  params.key_path_ = key_path;
  params.auto_increment_ = auto_increment;
  params.idb_database_id_ = idb_database_id;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseCreateObjectStore(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseRemoveObjectStore(
    const string16& name, WebIDBCallbacks* callbacks_ptr,
    int32 idb_database_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseRemoveObjectStore(
          idb_database_id, pending_callbacks_.Add(callbacks.release()), name));
}

void IndexedDBDispatcher::RequestIDBDatabaseSetVersion(
    const string16& version,
    WebIDBCallbacks* callbacks_ptr,
    int32 idb_database_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  RenderThread::current()->Send(
      new ViewHostMsg_IDBDatabaseSetVersion(
          idb_database_id, pending_callbacks_.Add(callbacks.release()),
                                                  version));
}

void IndexedDBDispatcher::RequestIDBObjectStoreGet(
    const IndexedDBKey& key, WebKit::WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreGet(
          idb_object_store_id, pending_callbacks_.Add(callbacks.release()),
          key));
}

void IndexedDBDispatcher::RequestIDBObjectStorePut(
    const SerializedScriptValue& value, const IndexedDBKey& key,
    bool add_only, WebKit::WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStorePut(
          idb_object_store_id, pending_callbacks_.Add(callbacks.release()),
          value, key, add_only));
}

void IndexedDBDispatcher::RequestIDBObjectStoreRemove(
    const IndexedDBKey& key, WebKit::WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreRemove(
          idb_object_store_id, pending_callbacks_.Add(callbacks.release()),
          key));
}

void IndexedDBDispatcher::RequestIDBObjectStoreCreateIndex(
    const string16& name, const NullableString16& key_path, bool unique,
    WebIDBCallbacks* callbacks_ptr, int32 idb_object_store_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  ViewHostMsg_IDBObjectStoreCreateIndex_Params params;
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.name_ = name;
  params.key_path_ = key_path;
  params.unique_ = unique;
  params.idb_object_store_id_ = idb_object_store_id;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreCreateIndex(params));
}

void IndexedDBDispatcher::RequestIDBObjectStoreRemoveIndex(
    const string16& name, WebIDBCallbacks* callbacks_ptr,
    int32 idb_object_store_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreRemoveIndex(
          idb_object_store_id, pending_callbacks_.Add(callbacks.release()),
          name));
}

void IndexedDBDispatcher::RequestIDBObjectStoreOpenCursor(
    const WebIDBKeyRange& idb_key_range, unsigned short direction,
    WebIDBCallbacks* callbacks_ptr, int32 idb_object_store_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  ViewHostMsg_IDBObjectStoreOpenCursor_Params params;
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.left_key_.Set(idb_key_range.left());
  params.right_key_.Set(idb_key_range.right());
  params.flags_ = idb_key_range.flags();
  params.direction_ = direction;
  params.idb_object_store_id_ = idb_object_store_id;
  RenderThread::current()->Send(
      new ViewHostMsg_IDBObjectStoreOpenCursor(params));
}

void IndexedDBDispatcher::RequestIDBTransactionSetCallbacks(
    WebKit::WebIDBTransactionCallbacks* callbacks) {
    pending_transaction_callbacks_.AddWithID(callbacks, callbacks->id());
}

void IndexedDBDispatcher::OnSuccessNull(int32 response_id) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess();
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIDBDatabase(int32 response_id,
                                               int32 object_id) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBDatabaseImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIndexedDBKey(int32 response_id,
                                                const IndexedDBKey& key) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(key);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIDBObjectStore(int32 response_id,
                                                  int32 object_id) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBObjectStoreImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessIDBIndex(int32 response_id,
                                            int32 object_id) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBIndexImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessSerializedScriptValue(
    int32 response_id, const SerializedScriptValue& value) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(value);
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessOpenCursor(int32 repsonse_id,
                                              int32 object_id) {
  WebKit::WebIDBCallbacks* callbacks =
        pending_callbacks_.Lookup(repsonse_id);
  callbacks->onSuccess(new RendererWebIDBCursorImpl(object_id));
  pending_callbacks_.Remove(repsonse_id);
}

void IndexedDBDispatcher::OnError(int32 response_id, int code,
                                  const string16& message) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onError(WebIDBDatabaseError(code, message));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnAbort(int transaction_id) {
  WebKit::WebIDBTransactionCallbacks* callbacks =
      pending_transaction_callbacks_.Lookup(transaction_id);
  DCHECK(callbacks);
  callbacks->onAbort();
  pending_transaction_callbacks_.Remove(transaction_id);
}

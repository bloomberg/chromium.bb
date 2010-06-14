// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indexed_db_dispatcher.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/renderer_webidbdatabase_impl.h"
#include "chrome/renderer/renderer_webidbindex_impl.h"
#include "chrome/renderer/renderer_webidbobjectstore_impl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabaseError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseError;

IndexedDBDispatcher::IndexedDBDispatcher() {
}

IndexedDBDispatcher::~IndexedDBDispatcher() {
}

bool IndexedDBDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IndexedDBDispatcher, msg)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbackSuccessReturnNull,
                        OnSuccessReturnNull)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbackSuccessCreateIDBDatabase,
                        OnSuccessCreateIDBDatabase)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbackSuccessCreateIDBObjectStore,
                        OnSuccessCreateIDBObjectStore)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbackSuccessCreateIDBIndex,
                        OnSuccessCreateIDBIndex)
    IPC_MESSAGE_HANDLER(ViewMsg_IDBCallbackError,
                        OnError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcher::RequestIndexedDatabaseOpen(
    const string16& name, const string16& description,
    WebIDBCallbacks* callbacks_ptr, const string16& origin,
    WebFrame* web_frame) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  if (!web_frame)
    return; // We must be shutting down.
  RenderView* render_view = RenderView::FromWebView(web_frame->view());
  if (!render_view)
    return; // We must be shutting down.

  ViewHostMsg_IndexedDatabaseOpen_Params params;
  params.routing_id_ = render_view->routing_id();
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.origin_ = origin;
  params.name_ = name;
  params.description_ = description;
  RenderThread::current()->Send(new ViewHostMsg_IndexedDatabaseOpen(params));
}

void IndexedDBDispatcher::RequestIDBDatabaseCreateObjectStore(
    const string16& name, const string16& keypath, bool auto_increment,
    WebIDBCallbacks* callbacks_ptr, int32 idb_database_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  ViewHostMsg_IDBDatabaseCreateObjectStore_Params params;
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.name_ = name;
  params.keypath_ = keypath;
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

void IndexedDBDispatcher::RequestIDBObjectStoreCreateIndex(
    const string16& name, const string16& keypath, bool unique,
    WebIDBCallbacks* callbacks_ptr, int32 idb_object_store_id) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);

  ViewHostMsg_IDBObjectStoreCreateIndex_Params params;
  params.response_id_ = pending_callbacks_.Add(callbacks.release());
  params.name_ = name;
  params.keypath_ = keypath;
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

void IndexedDBDispatcher::OnSuccessReturnNull(int32 response_id) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess();
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessCreateIDBDatabase(int32 response_id,
                                                     int32 object_id) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBDatabaseImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessCreateIDBObjectStore(int32 response_id,
                                                     int32 object_id) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBObjectStoreImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnSuccessCreateIDBIndex(int32 response_id,
                                               int32 object_id) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBIndexImpl(object_id));
  pending_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnError(int32 response_id, int code,
                                  const string16& message) {
  WebKit::WebIDBCallbacks* callbacks = pending_callbacks_.Lookup(response_id);
  callbacks->onError(WebIDBDatabaseError(code, message));
  pending_callbacks_.Remove(response_id);
}

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indexed_db_dispatcher.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/renderer_webidbdatabase_impl.h"
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
    IPC_MESSAGE_HANDLER(ViewMsg_IndexedDatabaseOpenSuccess,
                        OnIndexedDatabaseOpenSuccess)
    IPC_MESSAGE_HANDLER(ViewMsg_IndexedDatabaseOpenError,
                        OnIndexedDatabaseOpenError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcher::RequestIndexedDatabaseOpen(
    const string16& name, const string16& description, bool modify_database,
    WebIDBCallbacks* callbacks_ptr, const string16& origin, WebFrame* web_frame,
    int* exception_code) {
  scoped_ptr<WebIDBCallbacks> callbacks(callbacks_ptr);
  *exception_code = 0; // Redundant, but why not...

  if (!web_frame)
    return; // We must be shutting down.
  RenderView* render_view = RenderView::FromWebView(web_frame->view());
  if (!render_view)
    return; // We must be shutting down.

  ViewHostMsg_IndexedDatabaseOpen_Params params;
  params.routing_id_ = render_view->routing_id();
  params.response_id_ = indexed_database_open_callbacks_.Add(
      callbacks.release());
  params.origin_ = origin;
  params.name_ = name;
  params.description_ = description;
  params.modify_database_ = modify_database;
  RenderThread::current()->Send(new ViewHostMsg_IndexedDatabaseOpen(params));
}

void IndexedDBDispatcher::SendIDBDatabaseDestroyed(int32 idb_database_id) {
  // TODO(jorlow): Is it possible for this to be destroyed but still have
  //               pending callbacks?  If so, fix!
  RenderThread::current()->Send(new ViewHostMsg_IDBDatabaseDestroyed(
      idb_database_id));
}

void IndexedDBDispatcher::OnIndexedDatabaseOpenSuccess(
    int32 response_id, int32 idb_database_id) {
  WebKit::WebIDBCallbacks* callbacks =
      indexed_database_open_callbacks_.Lookup(response_id);
  callbacks->onSuccess(new RendererWebIDBDatabaseImpl(idb_database_id));
  indexed_database_open_callbacks_.Remove(response_id);
}

void IndexedDBDispatcher::OnIndexedDatabaseOpenError(
    int32 response_id, int code, const string16& message) {
  WebKit::WebIDBCallbacks* callbacks =
      indexed_database_open_callbacks_.Lookup(response_id);
  callbacks->onError(WebIDBDatabaseError(code, message));
  indexed_database_open_callbacks_.Remove(response_id);
}

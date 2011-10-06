// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webidbcursor_impl.h"

#include "content/common/indexed_db_messages.h"
#include "content/renderer/indexed_db_dispatcher.h"
#include "content/renderer/render_thread_impl.h"

using WebKit::WebExceptionCode;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBKey;
using WebKit::WebSerializedScriptValue;

RendererWebIDBCursorImpl::RendererWebIDBCursorImpl(int32 idb_cursor_id)
    : idb_cursor_id_(idb_cursor_id) {
}

RendererWebIDBCursorImpl::~RendererWebIDBCursorImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object wich owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  RenderThreadImpl::current()->Send(new IndexedDBHostMsg_CursorDestroyed(
      idb_cursor_id_));
}

unsigned short RendererWebIDBCursorImpl::direction() const {
  int direction;
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_CursorDirection(idb_cursor_id_, &direction));
  return direction;
}

WebIDBKey RendererWebIDBCursorImpl::key() const {
  IndexedDBKey key;
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_CursorKey(idb_cursor_id_, &key));
  return key;
}

WebIDBKey RendererWebIDBCursorImpl::primaryKey() const {
  IndexedDBKey primaryKey;
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_CursorPrimaryKey(idb_cursor_id_, &primaryKey));
  return primaryKey;
}

WebSerializedScriptValue RendererWebIDBCursorImpl::value() const {
  SerializedScriptValue scriptValue;
  RenderThreadImpl::current()->Send(
      new IndexedDBHostMsg_CursorValue(idb_cursor_id_, &scriptValue));
  return scriptValue;
}

void RendererWebIDBCursorImpl::update(const WebSerializedScriptValue& value,
                                      WebIDBCallbacks* callbacks,
                                      WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBCursorUpdate(SerializedScriptValue(value), callbacks,
                                     idb_cursor_id_, &ec);
}

void RendererWebIDBCursorImpl::continueFunction(const WebIDBKey& key,
                                                WebIDBCallbacks* callbacks,
                                                WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBCursorContinue(IndexedDBKey(key), callbacks,
                                       idb_cursor_id_, &ec);
}

void RendererWebIDBCursorImpl::deleteFunction(WebIDBCallbacks* callbacks,
                                              WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      RenderThreadImpl::current()->indexed_db_dispatcher();
  dispatcher->RequestIDBCursorDelete(callbacks, idb_cursor_id_, &ec);
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_callbacks.h"

#include "chrome/common/indexed_db_messages.h"

IndexedDBCallbacksBase::IndexedDBCallbacksBase(
    IndexedDBDispatcherHost* dispatcher_host,
    int32 response_id)
    : dispatcher_host_(dispatcher_host),
      response_id_(response_id) {
}

IndexedDBCallbacksBase::~IndexedDBCallbacksBase() {}

void IndexedDBCallbacksBase::onError(const WebKit::WebIDBDatabaseError& error) {
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksError(
      response_id_, error.code(), error.message()));
}

void IndexedDBCallbacksBase::onBlocked() {
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksBlocked(response_id_));
}

void IndexedDBCallbacks<WebKit::WebIDBCursor>::onSuccess(
    WebKit::WebIDBCursor* idb_object) {
  int32 object_id = dispatcher_host()->Add(idb_object);
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessIDBCursor(response_id(), object_id));
}

void IndexedDBCallbacks<WebKit::WebIDBCursor>::onSuccess(
    const WebKit::WebSerializedScriptValue& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessSerializedScriptValue(
          response_id(), SerializedScriptValue(value)));
}

void IndexedDBCallbacks<WebKit::WebIDBKey>::onSuccess(
    const WebKit::WebIDBKey& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessIndexedDBKey(
          response_id(), IndexedDBKey(value)));
}

void IndexedDBCallbacks<WebKit::WebSerializedScriptValue>::onSuccess(
    const WebKit::WebSerializedScriptValue& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessSerializedScriptValue(
          response_id(), SerializedScriptValue(value)));
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_callbacks.h"

#include "content/common/indexed_db_messages.h"
#include "webkit/quota/quota_manager.h"

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
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessIDBCursor(
      response_id(), object_id, IndexedDBKey(idb_object->key()),
      IndexedDBKey(idb_object->primaryKey()),
      content::SerializedScriptValue(idb_object->value())));
}

void IndexedDBCallbacks<WebKit::WebIDBCursor>::onSuccess(
    const WebKit::WebSerializedScriptValue& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessSerializedScriptValue(
          response_id(), content::SerializedScriptValue(value)));
}

void IndexedDBCallbacks<WebKit::WebIDBCursor>::onSuccessWithContinuation() {
  DCHECK(cursor_id_ != -1);
  WebKit::WebIDBCursor* idb_cursor = dispatcher_host()->GetCursorFromId(
      cursor_id_);

  DCHECK(idb_cursor);
  if (!idb_cursor)
    return;

  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessCursorContinue(
          response_id(),
          cursor_id_,
          IndexedDBKey(idb_cursor->key()),
          IndexedDBKey(idb_cursor->primaryKey()),
          content::SerializedScriptValue(idb_cursor->value())));
}

void IndexedDBCallbacks<WebKit::WebIDBCursor>::onSuccessWithPrefetch(
    const WebKit::WebVector<WebKit::WebIDBKey>& keys,
    const WebKit::WebVector<WebKit::WebIDBKey>& primaryKeys,
    const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values) {
  DCHECK(cursor_id_ != -1);

  std::vector<IndexedDBKey> msgKeys;
  std::vector<IndexedDBKey> msgPrimaryKeys;
  std::vector<content::SerializedScriptValue> msgValues;

  for (size_t i = 0; i < keys.size(); ++i) {
    msgKeys.push_back(IndexedDBKey(keys[i]));
    msgPrimaryKeys.push_back(IndexedDBKey(primaryKeys[i]));
    msgValues.push_back(content::SerializedScriptValue(values[i]));
  }

  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch(
          response_id(),
          cursor_id_,
          msgKeys,
          msgPrimaryKeys,
          msgValues));
}

void IndexedDBCallbacks<WebKit::WebIDBKey>::onSuccess(
    const WebKit::WebIDBKey& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessIndexedDBKey(
          response_id(), IndexedDBKey(value)));
}

void IndexedDBCallbacks<WebKit::WebDOMStringList>::onSuccess(
    const WebKit::WebDOMStringList& value) {

  std::vector<string16> list;
  for (unsigned i = 0; i < value.length(); ++i)
      list.push_back(value.item(i));

  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessStringList(
          response_id(), list));
}

void IndexedDBCallbacks<WebKit::WebSerializedScriptValue>::onSuccess(
    const WebKit::WebSerializedScriptValue& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessSerializedScriptValue(
          response_id(), content::SerializedScriptValue(value)));
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_callbacks.h"

#include <vector>

#include "content/common/indexed_db/indexed_db_messages.h"
#include "webkit/quota/quota_manager.h"

namespace content {

namespace {
const int32 kDatabaseNotAdded = -1;
}

IndexedDBCallbacksBase::IndexedDBCallbacksBase(
    IndexedDBDispatcherHost* dispatcher_host,
    int32 ipc_thread_id,
    int32 ipc_response_id)
    : dispatcher_host_(dispatcher_host),
      ipc_response_id_(ipc_response_id),
      ipc_thread_id_(ipc_thread_id) {
}

IndexedDBCallbacksBase::~IndexedDBCallbacksBase() {}

void IndexedDBCallbacksBase::onError(const WebKit::WebIDBDatabaseError& error) {
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksError(
      ipc_thread_id_, ipc_response_id_, error.code(), error.message()));
}

void IndexedDBCallbacksBase::onBlocked(long long old_version) {
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksIntBlocked(
      ipc_thread_id_, ipc_response_id_, old_version));
}

void IndexedDBCallbacksBase::onBlocked() {
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksBlocked(ipc_thread_id_,
                                                           ipc_response_id_));
}

IndexedDBCallbacksDatabase::IndexedDBCallbacksDatabase(
    IndexedDBDispatcherHost* dispatcher_host,
    int32 ipc_thread_id,
    int32 ipc_response_id,
    const GURL& origin_url)
    : IndexedDBCallbacksBase(dispatcher_host, ipc_thread_id, ipc_response_id),
      origin_url_(origin_url),
      ipc_database_id_(kDatabaseNotAdded) {
}

void IndexedDBCallbacksDatabase::onSuccess(
    WebKit::WebIDBDatabase* idb_object) {
  int32 ipc_object_id = ipc_database_id_;
  if (ipc_object_id == kDatabaseNotAdded) {
    ipc_object_id = dispatcher_host()->Add(idb_object, ipc_thread_id(),
                                           origin_url_);
  } else {
    // We already have this database and don't need a new copy of it.
    delete idb_object;
  }
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessIDBDatabase(ipc_thread_id(),
                                                   ipc_response_id(),
                                                   ipc_object_id));
}

void IndexedDBCallbacksDatabase::onUpgradeNeeded(
    long long old_version,
    WebKit::WebIDBTransaction* transaction,
    WebKit::WebIDBDatabase* database) {
  int32 ipc_transaction_id = dispatcher_host()->Add(transaction,
                                                    ipc_thread_id(),
                                                    origin_url_);
  int32 ipc_database_id = dispatcher_host()->Add(database, ipc_thread_id(),
                                                 origin_url_);
  ipc_database_id_ = ipc_database_id;
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksUpgradeNeeded(
          ipc_thread_id(), ipc_response_id(), ipc_transaction_id,
          ipc_database_id,
          old_version));
}

void IndexedDBCallbacks<WebKit::WebIDBCursor>::onSuccess(
    WebKit::WebIDBCursor* idb_object,
    const WebKit::WebIDBKey& key,
    const WebKit::WebIDBKey& primaryKey,
    const WebKit::WebSerializedScriptValue& value) {
  int32 ipc_object_id = dispatcher_host()->Add(idb_object);
  IndexedDBMsg_CallbacksSuccessIDBCursor_Params params;
  params.ipc_thread_id = ipc_thread_id();
  params.ipc_response_id = ipc_response_id();
  params.ipc_cursor_id = ipc_object_id;
  params.key = IndexedDBKey(key);
  params.primary_key = IndexedDBKey(primaryKey);
  params.serialized_value = SerializedScriptValue(value);
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessIDBCursor(params));
}

void IndexedDBCallbacks<WebKit::WebIDBCursor>::onSuccess(
    const WebKit::WebSerializedScriptValue& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessSerializedScriptValue(
          ipc_thread_id(), ipc_response_id(), SerializedScriptValue(value)));
}

void IndexedDBCallbacks<WebKit::WebIDBCursor>::onSuccess(
    const WebKit::WebIDBKey& key,
    const WebKit::WebIDBKey& primaryKey,
    const WebKit::WebSerializedScriptValue& value) {
  DCHECK_NE(ipc_cursor_id_, -1);
  WebKit::WebIDBCursor* idb_cursor = dispatcher_host()->GetCursorFromId(
      ipc_cursor_id_);

  DCHECK(idb_cursor);
  if (!idb_cursor)
    return;
  IndexedDBMsg_CallbacksSuccessCursorContinue_Params params;
  params.ipc_thread_id = ipc_thread_id();
  params.ipc_response_id = ipc_response_id();
  params.ipc_cursor_id = ipc_cursor_id_;
  params.key = IndexedDBKey(key);
  params.primary_key = IndexedDBKey(primaryKey);
  params.serialized_value = SerializedScriptValue(value);
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessCursorContinue(params));
}

void IndexedDBCallbacks<WebKit::WebIDBCursor>::onSuccessWithPrefetch(
    const WebKit::WebVector<WebKit::WebIDBKey>& keys,
    const WebKit::WebVector<WebKit::WebIDBKey>& primaryKeys,
    const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values) {
  DCHECK_NE(ipc_cursor_id_, -1);

  std::vector<IndexedDBKey> msgKeys;
  std::vector<IndexedDBKey> msgPrimaryKeys;
  std::vector<SerializedScriptValue> msgValues;

  for (size_t i = 0; i < keys.size(); ++i) {
    msgKeys.push_back(IndexedDBKey(keys[i]));
    msgPrimaryKeys.push_back(IndexedDBKey(primaryKeys[i]));
    msgValues.push_back(SerializedScriptValue(values[i]));
  }

  IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params params;
  params.ipc_thread_id = ipc_thread_id();
  params.ipc_response_id = ipc_response_id();
  params.ipc_cursor_id = ipc_cursor_id_;
  params.keys = msgKeys;
  params.primary_keys = msgPrimaryKeys;
  params.values = msgValues;
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch(params));
}

void IndexedDBCallbacks<WebKit::WebIDBKey>::onSuccess(
    const WebKit::WebIDBKey& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessIndexedDBKey(
          ipc_thread_id(), ipc_response_id(), IndexedDBKey(value)));
}

void IndexedDBCallbacks<WebKit::WebDOMStringList>::onSuccess(
    const WebKit::WebDOMStringList& value) {

  std::vector<string16> list;
  for (unsigned i = 0; i < value.length(); ++i)
      list.push_back(value.item(i));

  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessStringList(
          ipc_thread_id(), ipc_response_id(), list));
}

void IndexedDBCallbacks<WebKit::WebSerializedScriptValue>::onSuccess(
    const WebKit::WebSerializedScriptValue& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessSerializedScriptValue(
          ipc_thread_id(), ipc_response_id(), SerializedScriptValue(value)));
}

void IndexedDBCallbacks<WebKit::WebSerializedScriptValue>::onSuccess(
    const WebKit::WebSerializedScriptValue& value,
    const WebKit::WebIDBKey& primaryKey,
    const WebKit::WebIDBKeyPath& keyPath) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessSerializedScriptValueWithKey(
          ipc_thread_id(), ipc_response_id(), SerializedScriptValue(value),
          IndexedDBKey(primaryKey), IndexedDBKeyPath(keyPath)));
}

void IndexedDBCallbacks<WebKit::WebSerializedScriptValue>::onSuccess(
    long long value) {
    dispatcher_host()->Send(
        new IndexedDBMsg_CallbacksSuccessInteger(ipc_thread_id(),
                                                 ipc_response_id(),
                                                 value));
}

void IndexedDBCallbacks<WebKit::WebSerializedScriptValue>::onSuccess() {
    dispatcher_host()->Send(
        new IndexedDBMsg_CallbacksSuccessUndefined(ipc_thread_id(),
                                                   ipc_response_id()));
}

void IndexedDBCallbacks<WebKit::WebSerializedScriptValue>::onSuccess(
    const WebKit::WebIDBKey& value) {
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessIndexedDBKey(
          ipc_thread_id(), ipc_response_id(), IndexedDBKey(value)));
}

}  // namespace content

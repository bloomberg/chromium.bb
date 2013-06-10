// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_callbacks.h"

#include <vector>

#include "content/browser/indexed_db/webidbdatabase_impl.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "webkit/browser/quota/quota_manager.h"

using WebKit::WebData;
using WebKit::WebString;
using WebKit::WebVector;

namespace content {

namespace {
const int32 kDatabaseNotAdded = -1;
}

IndexedDBCallbacksBase::IndexedDBCallbacksBase(
    IndexedDBDispatcherHost* dispatcher_host,
    int32 ipc_thread_id,
    int32 ipc_callbacks_id)
    : dispatcher_host_(dispatcher_host),
      ipc_callbacks_id_(ipc_callbacks_id),
      ipc_thread_id_(ipc_thread_id) {}

IndexedDBCallbacksBase::~IndexedDBCallbacksBase() {}

void IndexedDBCallbacksBase::onError(const WebKit::WebIDBDatabaseError& error) {
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksError(
      ipc_thread_id_, ipc_callbacks_id_, error.code(), error.message()));
}

void IndexedDBCallbacksBase::onBlocked(long long old_version) {
  dispatcher_host_->Send(new IndexedDBMsg_CallbacksIntBlocked(
      ipc_thread_id_, ipc_callbacks_id_, old_version));
}

void IndexedDBCallbacksBase::onSuccess(
    const WebVector<WebString>& value) {
  NOTREACHED();
}

void IndexedDBCallbacksBase::onSuccess(WebIDBDatabaseImpl* idb_object,
                                       const WebKit::WebIDBMetadata& metadata) {
  NOTREACHED();
}

void IndexedDBCallbacksBase::onUpgradeNeeded(long long old_version,
                                             WebIDBDatabaseImpl* database,
                                             const WebKit::WebIDBMetadata&) {
  NOTREACHED();
}

void IndexedDBCallbacksBase::onSuccess(WebIDBCursorImpl* idb_object,
                                       const WebKit::WebIDBKey& key,
                                       const WebKit::WebIDBKey& primaryKey,
                                       const WebData& value) {
  NOTREACHED();
}

void IndexedDBCallbacksBase::onSuccess(const WebKit::WebIDBKey& key,
                                       const WebKit::WebIDBKey& primaryKey,
                                       const WebData& value) {
  NOTREACHED();
}

void IndexedDBCallbacksBase::onSuccess(const WebData& value) {
  NOTREACHED();
}

void IndexedDBCallbacksBase::onSuccessWithPrefetch(
    const WebVector<WebKit::WebIDBKey>& keys,
    const WebVector<WebKit::WebIDBKey>& primaryKeys,
    const WebVector<WebData>& values) {
  NOTREACHED();
}

void IndexedDBCallbacksBase::onSuccess(const WebKit::WebIDBKey& value) {
  NOTREACHED();
}

void IndexedDBCallbacksBase::onSuccess(const WebData& value,
                                       const WebKit::WebIDBKey& key,
                                       const WebKit::WebIDBKeyPath& keyPath) {
  NOTREACHED();
}

void IndexedDBCallbacksBase::onSuccess(long long value) { NOTREACHED(); }

void IndexedDBCallbacksBase::onSuccess() { NOTREACHED(); }

IndexedDBCallbacksDatabase::IndexedDBCallbacksDatabase(
    IndexedDBDispatcherHost* dispatcher_host,
    int32 ipc_thread_id,
    int32 ipc_callbacks_id,
    int32 ipc_database_callbacks_id,
    int64 host_transaction_id,
    const GURL& origin_url)
    : IndexedDBCallbacksBase(dispatcher_host, ipc_thread_id, ipc_callbacks_id),
      host_transaction_id_(host_transaction_id),
      origin_url_(origin_url),
      ipc_database_id_(kDatabaseNotAdded),
      ipc_database_callbacks_id_(ipc_database_callbacks_id) {}

void IndexedDBCallbacksDatabase::onSuccess(
    WebIDBDatabaseImpl* idb_object,
    const WebKit::WebIDBMetadata& metadata) {
  int32 ipc_object_id = ipc_database_id_;
  if (ipc_object_id == kDatabaseNotAdded) {
    ipc_object_id =
        dispatcher_host()->Add(idb_object, ipc_thread_id(), origin_url_);
  } else {
    // We already have this database and don't need a new copy of it.
    delete idb_object;
  }
  const ::IndexedDBDatabaseMetadata idb_metadata =
      IndexedDBDispatcherHost::ConvertMetadata(metadata);

  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessIDBDatabase(ipc_thread_id(),
                                                   ipc_callbacks_id(),
                                                   ipc_database_callbacks_id_,
                                                   ipc_object_id,
                                                   idb_metadata));
}

void IndexedDBCallbacksDatabase::onUpgradeNeeded(
    long long old_version,
    WebIDBDatabaseImpl* database,
    const WebKit::WebIDBMetadata& metadata) {
  dispatcher_host()->RegisterTransactionId(host_transaction_id_, origin_url_);
  int32 ipc_database_id =
      dispatcher_host()->Add(database, ipc_thread_id(), origin_url_);
  ipc_database_id_ = ipc_database_id;
  IndexedDBMsg_CallbacksUpgradeNeeded_Params params;
  params.ipc_thread_id = ipc_thread_id();
  params.ipc_callbacks_id = ipc_callbacks_id();
  params.ipc_database_id = ipc_database_id;
  params.ipc_database_callbacks_id = ipc_database_callbacks_id_;
  params.old_version = old_version;
  params.idb_metadata = IndexedDBDispatcherHost::ConvertMetadata(metadata);
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksUpgradeNeeded(params));
}

void IndexedDBCallbacks<WebIDBCursorImpl>::onSuccess(
    WebIDBCursorImpl* idb_cursor,
    const WebKit::WebIDBKey& key,
    const WebKit::WebIDBKey& primaryKey,
    const WebData& value) {
  int32 ipc_object_id = dispatcher_host()->Add(idb_cursor);
  IndexedDBMsg_CallbacksSuccessIDBCursor_Params params;
  params.ipc_thread_id = ipc_thread_id();
  params.ipc_callbacks_id = ipc_callbacks_id();
  params.ipc_cursor_id = ipc_object_id;
  params.key = IndexedDBKey(key);
  params.primary_key = IndexedDBKey(primaryKey);
  params.value.assign(value.data(), value.data() + value.size());
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessIDBCursor(params));
}

void IndexedDBCallbacks<WebIDBCursorImpl>::onSuccess(
    const WebData& webValue) {
  std::vector<char> value(webValue.data(), webValue.data() + webValue.size());
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessValue(
      ipc_thread_id(), ipc_callbacks_id(), value));
}

void IndexedDBCallbacks<WebIDBCursorImpl>::onSuccess(
    const WebKit::WebIDBKey& key,
    const WebKit::WebIDBKey& primaryKey,
    const WebData& value) {
  DCHECK_NE(ipc_cursor_id_, -1);
  WebIDBCursorImpl* idb_cursor =
      dispatcher_host()->GetCursorFromId(ipc_cursor_id_);

  DCHECK(idb_cursor);
  if (!idb_cursor)
    return;
  IndexedDBMsg_CallbacksSuccessCursorContinue_Params params;
  params.ipc_thread_id = ipc_thread_id();
  params.ipc_callbacks_id = ipc_callbacks_id();
  params.ipc_cursor_id = ipc_cursor_id_;
  params.key = IndexedDBKey(key);
  params.primary_key = IndexedDBKey(primaryKey);
  params.value.assign(value.data(), value.data() + value.size());
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessCursorContinue(params));
}

void IndexedDBCallbacks<WebIDBCursorImpl>::onSuccessWithPrefetch(
    const WebVector<WebKit::WebIDBKey>& keys,
    const WebVector<WebKit::WebIDBKey>& primaryKeys,
    const WebVector<WebData>& values) {
  DCHECK_NE(ipc_cursor_id_, -1);

  std::vector<IndexedDBKey> msgKeys;
  std::vector<IndexedDBKey> msgPrimaryKeys;
  std::vector<std::vector<char> > msgValues;

  for (size_t i = 0; i < keys.size(); ++i) {
    msgKeys.push_back(IndexedDBKey(keys[i]));
    msgPrimaryKeys.push_back(IndexedDBKey(primaryKeys[i]));
    msgValues.push_back(std::vector<char>(values[i].data(),
                                          values[i].data() + values[i].size()));
  }

  IndexedDBMsg_CallbacksSuccessCursorPrefetch_Params params;
  params.ipc_thread_id = ipc_thread_id();
  params.ipc_callbacks_id = ipc_callbacks_id();
  params.ipc_cursor_id = ipc_cursor_id_;
  params.keys = msgKeys;
  params.primary_keys = msgPrimaryKeys;
  params.values = msgValues;
  dispatcher_host()->Send(
      new IndexedDBMsg_CallbacksSuccessCursorPrefetch(params));
}

void IndexedDBCallbacks<WebKit::WebIDBKey>::onSuccess(
    const WebKit::WebIDBKey& value) {
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessIndexedDBKey(
      ipc_thread_id(), ipc_callbacks_id(), IndexedDBKey(value)));
}

void IndexedDBCallbacks<WebVector<WebString> >::onSuccess(
    const WebVector<WebString>& value) {

  std::vector<string16> list;
  for (unsigned i = 0; i < value.size(); ++i)
    list.push_back(value[i]);

  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessStringList(
      ipc_thread_id(), ipc_callbacks_id(), list));
}

void IndexedDBCallbacks<WebData>::onSuccess(
    const WebData& value) {
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessValue(
      ipc_thread_id(),
      ipc_callbacks_id(),
      std::vector<char>(value.data(), value.data() + value.size())));
}

void IndexedDBCallbacks<WebData>::onSuccess(
    const WebData& value,
    const WebKit::WebIDBKey& primaryKey,
    const WebKit::WebIDBKeyPath& keyPath) {
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessValueWithKey(
      ipc_thread_id(),
      ipc_callbacks_id(),
      std::vector<char>(value.data(), value.data() + value.size()),
      IndexedDBKey(primaryKey),
      IndexedDBKeyPath(keyPath)));
}

void IndexedDBCallbacks<WebData>::onSuccess(long long value) {
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessInteger(
      ipc_thread_id(), ipc_callbacks_id(), value));
}

void IndexedDBCallbacks<WebData>::onSuccess() {
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessUndefined(
      ipc_thread_id(), ipc_callbacks_id()));
}

void IndexedDBCallbacks<WebData>::onSuccess(
    const WebKit::WebIDBKey& value) {
  dispatcher_host()->Send(new IndexedDBMsg_CallbacksSuccessIndexedDBKey(
      ipc_thread_id(), ipc_callbacks_id(), IndexedDBKey(value)));
}

}  // namespace content

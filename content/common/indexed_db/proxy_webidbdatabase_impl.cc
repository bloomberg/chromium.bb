// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/proxy_webidbdatabase_impl.h"

#include <vector>

#include "content/common/child_thread.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/indexed_db/proxy_webidbtransaction_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyPath.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBMetadata.h"
#include "webkit/glue/worker_task_runner.h"

using WebKit::WebDOMStringList;
using WebKit::WebExceptionCode;
using WebKit::WebFrame;
using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabaseCallbacks;
using WebKit::WebIDBMetadata;
using WebKit::WebIDBKeyPath;
using WebKit::WebIDBTransaction;
using WebKit::WebString;
using WebKit::WebVector;
using webkit_glue::WorkerTaskRunner;

namespace content {

RendererWebIDBDatabaseImpl::RendererWebIDBDatabaseImpl(int32 ipc_database_id)
    : ipc_database_id_(ipc_database_id) {
}

RendererWebIDBDatabaseImpl::~RendererWebIDBDatabaseImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object which owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_DatabaseDestroyed(
      ipc_database_id_));
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->DatabaseDestroyed(ipc_database_id_);
}

WebIDBMetadata RendererWebIDBDatabaseImpl::metadata() const {
  IndexedDBDatabaseMetadata idb_metadata;
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_DatabaseMetadata(ipc_database_id_, &idb_metadata));

  WebIDBMetadata web_metadata;
  web_metadata.id = idb_metadata.id;
  web_metadata.name = idb_metadata.name;
  web_metadata.version = idb_metadata.version;
  web_metadata.intVersion = idb_metadata.int_version;
  web_metadata.maxObjectStoreId = idb_metadata.max_object_store_id;
  web_metadata.objectStores = WebVector<WebIDBMetadata::ObjectStore>(
      idb_metadata.object_stores.size());

  for (size_t i = 0; i < idb_metadata.object_stores.size(); ++i) {
    const IndexedDBObjectStoreMetadata& idb_store_metadata =
        idb_metadata.object_stores[i];
    WebIDBMetadata::ObjectStore& web_store_metadata =
        web_metadata.objectStores[i];

    web_store_metadata.id = idb_store_metadata.id;
    web_store_metadata.name = idb_store_metadata.name;
    web_store_metadata.keyPath = idb_store_metadata.keyPath;
    web_store_metadata.autoIncrement = idb_store_metadata.autoIncrement;
    web_store_metadata.maxIndexId = idb_store_metadata.max_index_id;
    web_store_metadata.indexes = WebVector<WebIDBMetadata::Index>(
        idb_store_metadata.indexes.size());

    for (size_t j = 0; j < idb_store_metadata.indexes.size(); ++j) {
      const IndexedDBIndexMetadata& idb_index_metadata =
          idb_store_metadata.indexes[j];
      WebIDBMetadata::Index& web_index_metadata =
          web_store_metadata.indexes[j];

      web_index_metadata.id = idb_index_metadata.id;
      web_index_metadata.name = idb_index_metadata.name;
      web_index_metadata.keyPath = idb_index_metadata.keyPath;
      web_index_metadata.unique = idb_index_metadata.unique;
      web_index_metadata.multiEntry = idb_index_metadata.multiEntry;
    }
  }

  return web_metadata;
}

void RendererWebIDBDatabaseImpl::createObjectStore(
    long long transaction_id,
    long long object_store_id,
    const WebKit::WebString& name,
    const WebKit::WebIDBKeyPath& key_path,
    bool auto_increment) {
  IndexedDBHostMsg_DatabaseCreateObjectStore_Params params;
  params.ipc_database_id = ipc_database_id_;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.name = name;
  params.key_path = IndexedDBKeyPath(key_path);
  params.auto_increment = auto_increment;

  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_DatabaseCreateObjectStore(params));
}

void RendererWebIDBDatabaseImpl::deleteObjectStore(
    long long transaction_id,
    long long object_store_id) {
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_DatabaseDeleteObjectStore(
          ipc_database_id_,
          transaction_id,
          object_store_id));
}

WebKit::WebIDBTransaction* RendererWebIDBDatabaseImpl::createTransaction(
    long long transaction_id,
    const WebVector<long long>& object_store_ids,
    unsigned short mode) {
  std::vector<int64> object_stores;
  object_stores.reserve(object_store_ids.size());
  for (unsigned int i = 0; i < object_store_ids.size(); ++i)
      object_stores.push_back(object_store_ids[i]);

  int ipc_transaction_id;
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_DatabaseCreateTransaction(
      WorkerTaskRunner::Instance()->CurrentWorkerId(),
      ipc_database_id_, transaction_id, object_stores, mode,
      &ipc_transaction_id));
  if (!transaction_id)
    return NULL;
  return new RendererWebIDBTransactionImpl(ipc_transaction_id);
}

void RendererWebIDBDatabaseImpl::close() {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabaseClose(ipc_database_id_);
}

void RendererWebIDBDatabaseImpl::get(
    long long transaction_id,
    long long object_store_id,
    long long index_id,
    const WebKit::WebIDBKeyRange& key_range,
    bool key_only,
    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabaseGet(
      ipc_database_id_, transaction_id, object_store_id, index_id,
      IndexedDBKeyRange(key_range), key_only, callbacks);
}

void RendererWebIDBDatabaseImpl::put(
    long long transaction_id,
    long long object_store_id,
    WebVector<unsigned char>* value,
    const WebKit::WebIDBKey& key,
    PutMode put_mode,
    WebIDBCallbacks* callbacks,
    const WebVector<long long>& web_index_ids,
    const WebVector<WebIndexKeys>& web_index_keys) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabasePut(
      ipc_database_id_, transaction_id, object_store_id,
      value, IndexedDBKey(key), put_mode, callbacks,
      web_index_ids, web_index_keys);
}

void RendererWebIDBDatabaseImpl::setIndexKeys(
    long long transaction_id,
    long long object_store_id,
    const WebKit::WebIDBKey& primary_key,
    const WebVector<long long>& index_ids,
    const WebVector<WebIndexKeys>& index_keys) {
  IndexedDBHostMsg_DatabaseSetIndexKeys_Params params;
  params.ipc_database_id = ipc_database_id_;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.primary_key = IndexedDBKey(primary_key);
  COMPILE_ASSERT(sizeof(params.index_ids[0]) ==
                 sizeof(index_ids[0]), Cant_copy);
  params.index_ids.assign(index_ids.data(),
                          index_ids.data() + index_ids.size());

  params.index_keys.resize(index_keys.size());
  for (size_t i = 0; i < index_keys.size(); ++i) {
    params.index_keys[i].resize(index_keys[i].size());
    for (size_t j = 0; j < index_keys[i].size(); ++j) {
      params.index_keys[i][j] = content::IndexedDBKey(index_keys[i][j]);
    }
  }
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_DatabaseSetIndexKeys(
      params));
}

void RendererWebIDBDatabaseImpl::setIndexesReady(
    long long transaction_id,
    long long object_store_id,
    const WebVector<long long>& web_index_ids) {
  std::vector<int64> index_ids(web_index_ids.data(),
                               web_index_ids.data() + web_index_ids.size());
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_DatabaseSetIndexesReady(
      ipc_database_id_, transaction_id, object_store_id, index_ids));
}

void RendererWebIDBDatabaseImpl::openCursor(
    long long transaction_id,
    long long object_store_id,
    long long index_id,
    const WebKit::WebIDBKeyRange& key_range,
    unsigned short direction,
    bool key_only,
    TaskType task_type,
    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabaseOpenCursor(
      ipc_database_id_,
      transaction_id, object_store_id, index_id,
      IndexedDBKeyRange(key_range), direction, key_only, task_type, callbacks);
}

void RendererWebIDBDatabaseImpl::count(
    long long transaction_id,
    long long object_store_id,
    long long index_id,
    const WebKit::WebIDBKeyRange& key_range,
    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabaseCount(
      ipc_database_id_,
      transaction_id, object_store_id, index_id,
      IndexedDBKeyRange(key_range), callbacks);
}

void RendererWebIDBDatabaseImpl::deleteRange(
    long long transaction_id,
    long long object_store_id,
    const WebKit::WebIDBKeyRange& key_range,
    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabaseDeleteRange(
      ipc_database_id_,
      transaction_id, object_store_id,
      IndexedDBKeyRange(key_range), callbacks);
}

void RendererWebIDBDatabaseImpl::clear(
    long long transaction_id,
    long long object_store_id,
    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabaseClear(
      ipc_database_id_,
      transaction_id, object_store_id, callbacks);
}

void RendererWebIDBDatabaseImpl::createIndex(
    long long transaction_id,
    long long object_store_id,
    long long index_id,
    const WebString& name,
    const WebIDBKeyPath& key_path,
    bool unique,
    bool multi_entry)
{
  IndexedDBHostMsg_DatabaseCreateIndex_Params params;
  params.ipc_database_id = ipc_database_id_;
  params.transaction_id = transaction_id;
  params.object_store_id = object_store_id;
  params.index_id = index_id;
  params.name = name;
  params.key_path = IndexedDBKeyPath(key_path);
  params.unique = unique;
  params.multi_entry = multi_entry;

  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_DatabaseCreateIndex(params));
}

void RendererWebIDBDatabaseImpl::deleteIndex(
    long long transaction_id,
    long long object_store_id,
    long long index_id)
{
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_DatabaseDeleteIndex(
          ipc_database_id_,
          transaction_id,
          object_store_id, index_id));
}
}  // namespace content

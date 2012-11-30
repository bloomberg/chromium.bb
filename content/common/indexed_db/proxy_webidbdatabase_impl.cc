// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/proxy_webidbdatabase_impl.h"

#include <vector>

#include "content/common/child_thread.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/indexed_db/proxy_webidbobjectstore_impl.h"
#include "content/common/indexed_db/proxy_webidbtransaction_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyPath.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBMetadata.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
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

RendererWebIDBDatabaseImpl::RendererWebIDBDatabaseImpl(int32 idb_database_id)
    : idb_database_id_(idb_database_id) {
}

RendererWebIDBDatabaseImpl::~RendererWebIDBDatabaseImpl() {
  // It's not possible for there to be pending callbacks that address this
  // object since inside WebKit, they hold a reference to the object which owns
  // this object. But, if that ever changed, then we'd need to invalidate
  // any such pointers.
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_DatabaseDestroyed(
      idb_database_id_));
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->DatabaseDestroyed(idb_database_id_);
}

WebIDBMetadata RendererWebIDBDatabaseImpl::metadata() const {
  IndexedDBDatabaseMetadata idb_metadata;
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_DatabaseMetadata(idb_database_id_, &idb_metadata));

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

WebKit::WebIDBObjectStore* RendererWebIDBDatabaseImpl::createObjectStore(
    long long id,
    const WebKit::WebString& name,
    const WebKit::WebIDBKeyPath& key_path,
    bool auto_increment,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBHostMsg_DatabaseCreateObjectStore_Params params;
  params.id = id;
  params.name = name;
  params.key_path = IndexedDBKeyPath(key_path);
  params.auto_increment = auto_increment;
  params.transaction_id = IndexedDBDispatcher::TransactionId(transaction);
  params.idb_database_id = idb_database_id_;

  int object_store;
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_DatabaseCreateObjectStore(
          params, &object_store, &ec));
  if (!object_store)
    return NULL;
  return new RendererWebIDBObjectStoreImpl(object_store);
}

void RendererWebIDBDatabaseImpl::deleteObjectStore(
    long long object_store_id,
    const WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_DatabaseDeleteObjectStore(
          idb_database_id_, object_store_id,
          IndexedDBDispatcher::TransactionId(transaction), &ec));
}

WebKit::WebIDBTransaction* RendererWebIDBDatabaseImpl::createTransaction(
    long long transaction_id,
    const WebVector<long long>& object_store_ids,
    unsigned short mode) {
  std::vector<int64> object_stores;
  object_stores.reserve(object_store_ids.size());
  for (unsigned int i = 0; i < object_store_ids.size(); ++i)
      object_stores.push_back(object_store_ids[i]);

  int idb_transaction_id;
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_DatabaseCreateTransaction(
      WorkerTaskRunner::Instance()->CurrentWorkerId(),
      idb_database_id_, transaction_id, object_stores, mode,
      &idb_transaction_id));
  if (!transaction_id)
    return NULL;
  return new RendererWebIDBTransactionImpl(idb_transaction_id);
}

void RendererWebIDBDatabaseImpl::close() {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabaseClose(idb_database_id_);
}

}  // namespace content

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/proxy_webidbdatabase_impl.h"

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
  web_metadata.name = idb_metadata.name;
  web_metadata.version = idb_metadata.version;
  web_metadata.intVersion = idb_metadata.intVersion;
  web_metadata.objectStores = WebVector<WebIDBMetadata::ObjectStore>(
      idb_metadata.object_stores.size());

  for (size_t i = 0; i < idb_metadata.object_stores.size(); ++i) {
    const IndexedDBObjectStoreMetadata& idb_store_metadata =
        idb_metadata.object_stores[i];
    WebIDBMetadata::ObjectStore& web_store_metadata =
        web_metadata.objectStores[i];

    web_store_metadata.name = idb_store_metadata.name;
    web_store_metadata.keyPath = idb_store_metadata.keyPath;
    web_store_metadata.autoIncrement = idb_store_metadata.autoIncrement;
    web_store_metadata.indexes = WebVector<WebIDBMetadata::Index>(
        idb_store_metadata.indexes.size());

    for (size_t j = 0; j < idb_store_metadata.indexes.size(); ++j) {
      const IndexedDBIndexMetadata& idb_index_metadata =
          idb_store_metadata.indexes[j];
      WebIDBMetadata::Index& web_index_metadata =
          web_store_metadata.indexes[j];

      web_index_metadata.name = idb_index_metadata.name;
      web_index_metadata.keyPath = idb_index_metadata.keyPath;
      web_index_metadata.unique = idb_index_metadata.unique;
      web_index_metadata.multiEntry = idb_index_metadata.multiEntry;
    }
  }

  return web_metadata;
}

WebKit::WebIDBObjectStore* RendererWebIDBDatabaseImpl::createObjectStore(
    const WebKit::WebString& name,
    const WebKit::WebIDBKeyPath& key_path,
    bool auto_increment,
    const WebKit::WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBHostMsg_DatabaseCreateObjectStore_Params params;
  params.name = name;
  params.key_path = content::IndexedDBKeyPath(key_path);
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
    const WebString& name,
    const WebIDBTransaction& transaction,
    WebExceptionCode& ec) {
  IndexedDBDispatcher::Send(
      new IndexedDBHostMsg_DatabaseDeleteObjectStore(
          idb_database_id_, name,
          IndexedDBDispatcher::TransactionId(transaction), &ec));
}

void RendererWebIDBDatabaseImpl::setVersion(
    const WebString& version,
    WebIDBCallbacks* callbacks,
    WebExceptionCode& ec) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabaseSetVersion(
      version, callbacks, idb_database_id_, &ec);
}

WebKit::WebIDBTransaction* RendererWebIDBDatabaseImpl::transaction(
    const WebDOMStringList& names,
    unsigned short mode,
    WebExceptionCode& ec) {
  std::vector<string16> object_stores;
  object_stores.reserve(names.length());
  for (unsigned int i = 0; i < names.length(); ++i)
    object_stores.push_back(names.item(i));

  int transaction_id;
  IndexedDBDispatcher::Send(new IndexedDBHostMsg_DatabaseTransaction(
      WorkerTaskRunner::Instance()->CurrentWorkerId(),
      idb_database_id_, object_stores, mode, &transaction_id, &ec));
  if (!transaction_id)
    return NULL;
  return new RendererWebIDBTransactionImpl(transaction_id);
}

void RendererWebIDBDatabaseImpl::close() {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  dispatcher->RequestIDBDatabaseClose(idb_database_id_);
}

// TODO(jsbell): Remove once WK90411 has rolled.
void RendererWebIDBDatabaseImpl::open(WebIDBDatabaseCallbacks* callbacks) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->RequestIDBDatabaseOpen(callbacks, idb_database_id_);
}

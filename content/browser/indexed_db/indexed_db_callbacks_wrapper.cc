// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callbacks_wrapper.h"

#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/indexed_db/webidbcursor_impl.h"
#include "content/browser/indexed_db/webidbdatabase_impl.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/WebIDBKey.h"
#include "third_party/WebKit/public/platform/WebIDBMetadata.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

using WebKit::WebData;
using WebKit::WebIDBKey;
using WebKit::WebIDBMetadata;
using WebKit::WebString;
using WebKit::WebVector;

IndexedDBCallbacksWrapper::IndexedDBCallbacksWrapper(
    WebKit::WebIDBCallbacks* callbacks)
    : callbacks_(callbacks), did_complete_(false), did_create_proxy_(false) {}

IndexedDBCallbacksWrapper::~IndexedDBCallbacksWrapper() {}

void IndexedDBCallbacksWrapper::OnError(const IndexedDBDatabaseError& error) {
  DCHECK(callbacks_);
  callbacks_->onError(
      WebKit::WebIDBDatabaseError(error.code(), error.message()));
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(const std::vector<string16>& value) {
  DCHECK(callbacks_);
  callbacks_->onSuccess(WebVector<WebString>(value));
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(scoped_refptr<IndexedDBCursor> cursor,
                                          const IndexedDBKey& key,
                                          const IndexedDBKey& primary_key,
                                          std::vector<char>* value) {
  DCHECK(callbacks_);
  WebData web_value;
  if (value && value->size())
    web_value.assign(&value->front(), value->size());
  callbacks_->onSuccess(
      new WebIDBCursorImpl(cursor), WebIDBKey(key), primary_key, web_value);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(const IndexedDBKey& key) {
  DCHECK(callbacks_);
  callbacks_->onSuccess(WebIDBKey(key));
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(std::vector<char>* value) {
  WebData web_value;
  if (value && value->size())
    web_value.assign(&value->front(), value->size());

  DCHECK(callbacks_);
  callbacks_->onSuccess(web_value);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(std::vector<char>* value,
                                          const IndexedDBKey& key,
                                          const IndexedDBKeyPath& key_path) {
  WebData web_value;
  if (value && value->size())
    web_value.assign(&value->front(), value->size());
  DCHECK(callbacks_);
  callbacks_->onSuccess(web_value, WebIDBKey(key), key_path);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(int64 value) {
  DCHECK(callbacks_);
  callbacks_->onSuccess(value);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess() {
  DCHECK(callbacks_);
  callbacks_->onSuccess();
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(const IndexedDBKey& key,
                                          const IndexedDBKey& primary_key,
                                          std::vector<char>* value) {
  WebData web_value;
  if (value && value->size())
    web_value.assign(&value->front(), value->size());
  DCHECK(callbacks_);
  callbacks_->onSuccess(key, primary_key, web_value);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccessWithPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    const std::vector<std::vector<char> >& values) {
  DCHECK_EQ(keys.size(), primary_keys.size());
  DCHECK_EQ(keys.size(), values.size());

  std::vector<WebIDBKey> web_keys(keys.size());
  std::vector<WebIDBKey> web_primary_keys(primary_keys.size());
  std::vector<WebData> web_values(values.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    web_keys[i] = keys[i];
    web_primary_keys[i] = primary_keys[i];
    if (values[i].size())
      web_values[i].assign(&values[i].front(), values[i].size());
  }

  DCHECK(callbacks_);
  callbacks_->onSuccessWithPrefetch(web_keys, web_primary_keys, web_values);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnBlocked(int64 existing_version) {
  DCHECK(callbacks_);
  callbacks_->onBlocked(existing_version);
}

WebIDBMetadata ConvertMetadata(const IndexedDBDatabaseMetadata& idb_metadata) {
  WebIDBMetadata web_metadata;
  web_metadata.id = idb_metadata.id;
  web_metadata.name = idb_metadata.name;
  web_metadata.version = idb_metadata.version;
  web_metadata.intVersion = idb_metadata.int_version;
  web_metadata.maxObjectStoreId = idb_metadata.max_object_store_id;
  web_metadata.objectStores =
      WebVector<WebIDBMetadata::ObjectStore>(idb_metadata.object_stores.size());

  size_t i = 0;
  for (IndexedDBDatabaseMetadata::ObjectStoreMap::const_iterator
           it = idb_metadata.object_stores.begin();
       it != idb_metadata.object_stores.end();
       ++it, ++i) {
    const IndexedDBObjectStoreMetadata& idb_store_metadata = it->second;
    WebIDBMetadata::ObjectStore& web_store_metadata =
        web_metadata.objectStores[i];

    web_store_metadata.id = idb_store_metadata.id;
    web_store_metadata.name = idb_store_metadata.name;
    web_store_metadata.keyPath = idb_store_metadata.key_path;
    web_store_metadata.autoIncrement = idb_store_metadata.auto_increment;
    web_store_metadata.maxIndexId = idb_store_metadata.max_index_id;
    web_store_metadata.indexes =
        WebVector<WebIDBMetadata::Index>(idb_store_metadata.indexes.size());

    size_t j = 0;
    for (IndexedDBObjectStoreMetadata::IndexMap::const_iterator
             it2 = idb_store_metadata.indexes.begin();
         it2 != idb_store_metadata.indexes.end();
         ++it2, ++j) {
      const IndexedDBIndexMetadata& idb_index_metadata = it2->second;
      WebIDBMetadata::Index& web_index_metadata = web_store_metadata.indexes[j];

      web_index_metadata.id = idb_index_metadata.id;
      web_index_metadata.name = idb_index_metadata.name;
      web_index_metadata.keyPath = idb_index_metadata.key_path;
      web_index_metadata.unique = idb_index_metadata.unique;
      web_index_metadata.multiEntry = idb_index_metadata.multi_entry;
    }
  }

  return web_metadata;
}

void IndexedDBCallbacksWrapper::OnUpgradeNeeded(
    int64 old_version,
    scoped_refptr<IndexedDBDatabase> database,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(callbacks_);
  WebIDBMetadata web_metadata = ConvertMetadata(metadata);
  did_create_proxy_ = true;
  callbacks_->onUpgradeNeeded(
      old_version,
      new WebIDBDatabaseImpl(database, database_callbacks_),
      web_metadata);
  database_callbacks_ = NULL;
}

void IndexedDBCallbacksWrapper::OnSuccess(
    scoped_refptr<IndexedDBDatabase> database,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(callbacks_);
  WebIDBMetadata web_metadata = ConvertMetadata(metadata);
  scoped_refptr<IndexedDBCallbacksWrapper> self(this);

  WebIDBDatabaseImpl* impl =
      did_create_proxy_ ? 0
                        : new WebIDBDatabaseImpl(database, database_callbacks_);
  database_callbacks_ = NULL;

  callbacks_->onSuccess(impl, web_metadata);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::SetDatabaseCallbacks(
    scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks) {
  database_callbacks_ = database_callbacks;
}
}  // namespace content

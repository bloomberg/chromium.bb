// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/webidbdatabase_impl.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_callbacks_wrapper.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/WebIDBKey.h"
#include "third_party/WebKit/public/platform/WebIDBKeyRange.h"
#include "third_party/WebKit/public/platform/WebIDBMetadata.h"

using WebKit::WebString;
using WebKit::WebIDBKey;
using WebKit::WebData;
using WebKit::WebIDBKeyPath;
using WebKit::WebIDBKeyRange;
using WebKit::WebIDBDatabaseCallbacks;
using WebKit::WebIDBCallbacks;
using WebKit::WebVector;
using WebKit::WebIDBDatabaseError;

namespace content {

WebIDBDatabaseImpl::WebIDBDatabaseImpl(
    scoped_refptr<IndexedDBDatabase> database_backend,
    scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks)
    : database_backend_(database_backend),
      database_callbacks_(database_callbacks) {}

WebIDBDatabaseImpl::~WebIDBDatabaseImpl() {}

void WebIDBDatabaseImpl::createObjectStore(long long transaction_id,
                                           long long object_store_id,
                                           const WebString& name,
                                           const WebIDBKeyPath& key_path,
                                           bool auto_increment) {
  database_backend_->CreateObjectStore(transaction_id,
                                       object_store_id,
                                       name,
                                       IndexedDBKeyPath(key_path),
                                       auto_increment);
}

void WebIDBDatabaseImpl::deleteObjectStore(long long transaction_id,
                                           long long object_store_id) {
  database_backend_->DeleteObjectStore(transaction_id, object_store_id);
}

void WebIDBDatabaseImpl::createTransaction(
    long long id,
    WebIDBDatabaseCallbacks* /*callbacks*/,
    const WebVector<long long>& object_store_ids,
    unsigned short mode) {
  if (!database_callbacks_)
    return;
  std::vector<int64> object_store_id_list(object_store_ids.size());
  for (size_t i = 0; i < object_store_ids.size(); ++i)
    object_store_id_list[i] = object_store_ids[i];
  database_backend_->CreateTransaction(
      id, database_callbacks_.get(), object_store_id_list, mode);
}

void WebIDBDatabaseImpl::close() {
  // Use the callbacks passed in to the constructor so that the backend in
  // multi-process chromium knows which database connection is closing.
  if (!database_callbacks_)
    return;
  database_backend_->Close(database_callbacks_);
  database_callbacks_ = NULL;
}

void WebIDBDatabaseImpl::forceClose() {
  if (!database_callbacks_)
    return;
  database_backend_->Close(database_callbacks_);
  database_callbacks_->OnForcedClose();
  database_callbacks_ = NULL;
}

void WebIDBDatabaseImpl::abort(long long transaction_id) {
  if (database_backend_)
    database_backend_->Abort(transaction_id);
}

void WebIDBDatabaseImpl::abort(long long transaction_id,
                               const WebIDBDatabaseError& error) {
  if (database_backend_)
    database_backend_->Abort(transaction_id,
                             IndexedDBDatabaseError::Create(error));
}

void WebIDBDatabaseImpl::commit(long long transaction_id) {
  if (database_backend_)
    database_backend_->Commit(transaction_id);
}

void WebIDBDatabaseImpl::openCursor(long long transaction_id,
                                    long long object_store_id,
                                    long long index_id,
                                    const WebIDBKeyRange& key_range,
                                    unsigned short direction,
                                    bool key_only,
                                    TaskType task_type,
                                    WebIDBCallbacks* callbacks) {
  if (database_backend_)
    database_backend_->OpenCursor(
        transaction_id,
        object_store_id,
        index_id,
        make_scoped_ptr(new IndexedDBKeyRange(key_range)),
        static_cast<indexed_db::CursorDirection>(direction),
        key_only,
        static_cast<IndexedDBDatabase::TaskType>(task_type),
        IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBDatabaseImpl::count(long long transaction_id,
                               long long object_store_id,
                               long long index_id,
                               const WebIDBKeyRange& key_range,
                               WebIDBCallbacks* callbacks) {
  if (database_backend_)
    database_backend_->Count(transaction_id,
                             object_store_id,
                             index_id,
                             make_scoped_ptr(new IndexedDBKeyRange(key_range)),
                             IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBDatabaseImpl::get(long long transaction_id,
                             long long object_store_id,
                             long long index_id,
                             const WebIDBKeyRange& key_range,
                             bool key_only,
                             WebIDBCallbacks* callbacks) {
  if (database_backend_)
    database_backend_->Get(transaction_id,
                           object_store_id,
                           index_id,
                           make_scoped_ptr(new IndexedDBKeyRange(key_range)),
                           key_only,
                           IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBDatabaseImpl::put(long long transaction_id,
                             long long object_store_id,
                             const WebData& value,
                             const WebIDBKey& key,
                             PutMode put_mode,
                             WebIDBCallbacks* callbacks,
                             const WebVector<long long>& web_index_ids,
                             const WebVector<WebIndexKeys>& web_index_keys) {
  if (!database_backend_)
    return;

  DCHECK_EQ(web_index_ids.size(), web_index_keys.size());
  std::vector<int64> index_ids(web_index_ids.size());
  std::vector<IndexedDBDatabase::IndexKeys> index_keys(web_index_keys.size());

  for (size_t i = 0; i < web_index_ids.size(); ++i) {
    index_ids[i] = web_index_ids[i];
    IndexedDBKey::KeyArray index_key_list;
    for (size_t j = 0; j < web_index_keys[i].size(); ++j)
      index_key_list.push_back(IndexedDBKey(web_index_keys[i][j]));
    index_keys[i] = index_key_list;
  }

  std::vector<char> value_buffer(value.data(), value.data() + value.size());
  database_backend_->Put(transaction_id,
                         object_store_id,
                         &value_buffer,
                         make_scoped_ptr(new IndexedDBKey(key)),
                         static_cast<IndexedDBDatabase::PutMode>(put_mode),
                         IndexedDBCallbacksWrapper::Create(callbacks),
                         index_ids,
                         index_keys);
}

void WebIDBDatabaseImpl::setIndexKeys(
    long long transaction_id,
    long long object_store_id,
    const WebIDBKey& primary_key,
    const WebVector<long long>& web_index_ids,
    const WebVector<WebIndexKeys>& web_index_keys) {
  if (!database_backend_)
    return;

  DCHECK_EQ(web_index_ids.size(), web_index_keys.size());
  std::vector<int64> index_ids(web_index_ids.size());
  std::vector<IndexedDBDatabase::IndexKeys> index_keys(web_index_keys.size());

  for (size_t i = 0; i < web_index_ids.size(); ++i) {
    index_ids[i] = web_index_ids[i];
    IndexedDBKey::KeyArray index_key_list;
    for (size_t j = 0; j < web_index_keys[i].size(); ++j)
      index_key_list.push_back(IndexedDBKey(web_index_keys[i][j]));
    index_keys[i] = index_key_list;
  }
  database_backend_->SetIndexKeys(
      transaction_id,
      object_store_id,
      make_scoped_ptr(new IndexedDBKey(primary_key)),
      index_ids,
      index_keys);
}

void WebIDBDatabaseImpl::setIndexesReady(
    long long transaction_id,
    long long object_store_id,
    const WebVector<long long>& web_index_ids) {
  if (!database_backend_)
    return;

  std::vector<int64> index_ids(web_index_ids.size());
  for (size_t i = 0; i < web_index_ids.size(); ++i)
    index_ids[i] = web_index_ids[i];
  database_backend_->SetIndexesReady(
      transaction_id, object_store_id, index_ids);
}

void WebIDBDatabaseImpl::deleteRange(long long transaction_id,
                                     long long object_store_id,
                                     const WebIDBKeyRange& key_range,
                                     WebIDBCallbacks* callbacks) {
  if (database_backend_)
    database_backend_->DeleteRange(
        transaction_id,
        object_store_id,
        make_scoped_ptr(new IndexedDBKeyRange(key_range)),
        IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBDatabaseImpl::clear(long long transaction_id,
                               long long object_store_id,
                               WebIDBCallbacks* callbacks) {
  if (database_backend_)
    database_backend_->Clear(transaction_id,
                             object_store_id,
                             IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBDatabaseImpl::createIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id,
                                     const WebString& name,
                                     const WebIDBKeyPath& key_path,
                                     bool unique,
                                     bool multi_entry) {
  if (database_backend_)
    database_backend_->CreateIndex(transaction_id,
                                   object_store_id,
                                   index_id,
                                   name,
                                   IndexedDBKeyPath(key_path),
                                   unique,
                                   multi_entry);
}

void WebIDBDatabaseImpl::deleteIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id) {
  if (database_backend_)
    database_backend_->DeleteIndex(transaction_id, object_store_id, index_id);
}

}  // namespace WebKit

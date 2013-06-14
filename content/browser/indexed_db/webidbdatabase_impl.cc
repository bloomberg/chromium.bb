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
#include "third_party/WebKit/public/platform/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/WebString.h"

using WebKit::WebString;
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
                                           const IndexedDBKeyPath& key_path,
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
    IndexedDBDatabaseCallbacks* /*callbacks*/,
    const std::vector<int64>& object_store_ids,
    unsigned short mode) {
  if (!database_callbacks_.get())
    return;
  database_backend_->CreateTransaction(
      id, database_callbacks_.get(), object_store_ids, mode);
}

void WebIDBDatabaseImpl::close() {
  // Use the callbacks passed in to the constructor so that the backend in
  // multi-process chromium knows which database connection is closing.
  if (!database_callbacks_.get())
    return;
  database_backend_->Close(database_callbacks_);
  database_callbacks_ = NULL;
}

void WebIDBDatabaseImpl::forceClose() {
  if (!database_callbacks_.get())
    return;
  database_backend_->Close(database_callbacks_);
  database_callbacks_->OnForcedClose();
  database_callbacks_ = NULL;
}

void WebIDBDatabaseImpl::abort(long long transaction_id) {
  if (database_backend_.get())
    database_backend_->Abort(transaction_id);
}

void WebIDBDatabaseImpl::abort(long long transaction_id,
                               const WebIDBDatabaseError& error) {
  if (database_backend_.get())
    database_backend_->Abort(transaction_id, IndexedDBDatabaseError(error));
}

void WebIDBDatabaseImpl::commit(long long transaction_id) {
  if (database_backend_.get())
    database_backend_->Commit(transaction_id);
}

void WebIDBDatabaseImpl::openCursor(long long transaction_id,
                                    long long object_store_id,
                                    long long index_id,
                                    const IndexedDBKeyRange& key_range,
                                    unsigned short direction,
                                    bool key_only,
                                    WebKit::WebIDBDatabase::TaskType task_type,
                                    IndexedDBCallbacksBase* callbacks) {
  if (database_backend_.get())
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
                               const IndexedDBKeyRange& key_range,
                               IndexedDBCallbacksBase* callbacks) {
  if (database_backend_.get())
    database_backend_->Count(transaction_id,
                             object_store_id,
                             index_id,
                             make_scoped_ptr(new IndexedDBKeyRange(key_range)),
                             IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBDatabaseImpl::get(long long transaction_id,
                             long long object_store_id,
                             long long index_id,
                             const IndexedDBKeyRange& key_range,
                             bool key_only,
                             IndexedDBCallbacksBase* callbacks) {
  if (database_backend_.get())
    database_backend_->Get(transaction_id,
                           object_store_id,
                           index_id,
                           make_scoped_ptr(new IndexedDBKeyRange(key_range)),
                           key_only,
                           IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBDatabaseImpl::put(long long transaction_id,
                             long long object_store_id,
                             std::vector<char>* value,
                             const IndexedDBKey& key,
                             WebKit::WebIDBDatabase::PutMode put_mode,
                             IndexedDBCallbacksBase* callbacks,
                             const std::vector<int64>& index_ids,
                             const std::vector<IndexKeys>& index_keys) {
  if (!database_backend_.get())
    return;

  DCHECK_EQ(index_ids.size(), index_keys.size());

  database_backend_->Put(transaction_id,
                         object_store_id,
                         value,
                         make_scoped_ptr(new IndexedDBKey(key)),
                         static_cast<IndexedDBDatabase::PutMode>(put_mode),
                         IndexedDBCallbacksWrapper::Create(callbacks),
                         index_ids,
                         index_keys);
}

void WebIDBDatabaseImpl::setIndexKeys(
    long long transaction_id,
    long long object_store_id,
    const IndexedDBKey& primary_key,
    const std::vector<int64>& index_ids,
    const std::vector<IndexKeys>& index_keys) {
  if (!database_backend_.get())
    return;

  DCHECK_EQ(index_ids.size(), index_keys.size());
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
    const std::vector<int64>& web_index_ids) {
  if (!database_backend_.get())
    return;

  std::vector<int64> index_ids(web_index_ids.size());
  for (size_t i = 0; i < web_index_ids.size(); ++i)
    index_ids[i] = web_index_ids[i];
  database_backend_->SetIndexesReady(
      transaction_id, object_store_id, index_ids);
}

void WebIDBDatabaseImpl::deleteRange(long long transaction_id,
                                     long long object_store_id,
                                     const IndexedDBKeyRange& key_range,
                                     IndexedDBCallbacksBase* callbacks) {
  if (database_backend_.get())
    database_backend_->DeleteRange(
        transaction_id,
        object_store_id,
        make_scoped_ptr(new IndexedDBKeyRange(key_range)),
        IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBDatabaseImpl::clear(long long transaction_id,
                               long long object_store_id,
                               IndexedDBCallbacksBase* callbacks) {
  if (database_backend_.get())
    database_backend_->Clear(transaction_id,
                             object_store_id,
                             IndexedDBCallbacksWrapper::Create(callbacks));
}

void WebIDBDatabaseImpl::createIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id,
                                     const WebString& name,
                                     const IndexedDBKeyPath& key_path,
                                     bool unique,
                                     bool multi_entry) {
  if (database_backend_.get())
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
  if (database_backend_.get())
    database_backend_->DeleteIndex(transaction_id, object_store_id, index_id);
}

}  // namespace WebKit

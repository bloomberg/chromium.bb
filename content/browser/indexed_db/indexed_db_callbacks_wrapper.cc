// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_callbacks_wrapper.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/indexed_db/webidbcursor_impl.h"
#include "content/browser/indexed_db/webidbdatabase_impl.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseError.h"

namespace content {

IndexedDBCallbacksWrapper::IndexedDBCallbacksWrapper(
    IndexedDBCallbacksBase* callbacks)
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
  callbacks_->onSuccess(value);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(scoped_refptr<IndexedDBCursor> cursor,
                                          const IndexedDBKey& key,
                                          const IndexedDBKey& primary_key,
                                          std::vector<char>* value) {
  DCHECK(callbacks_);
  callbacks_->onSuccess(new WebIDBCursorImpl(cursor), key, primary_key, value);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(const IndexedDBKey& key) {
  DCHECK(callbacks_);
  callbacks_->onSuccess(key);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(std::vector<char>* value) {
  DCHECK(callbacks_);
  callbacks_->onSuccess(value);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccess(std::vector<char>* value,
                                          const IndexedDBKey& key,
                                          const IndexedDBKeyPath& key_path) {
  DCHECK(callbacks_);
  callbacks_->onSuccess(value, key, key_path);
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
  DCHECK(callbacks_);
  callbacks_->onSuccess(key, primary_key, value);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnSuccessWithPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    const std::vector<std::vector<char> >& values) {
  DCHECK_EQ(keys.size(), primary_keys.size());
  DCHECK_EQ(keys.size(), values.size());

  DCHECK(callbacks_);
  callbacks_->onSuccessWithPrefetch(keys, primary_keys, values);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::OnBlocked(int64 existing_version) {
  DCHECK(callbacks_);
  callbacks_->onBlocked(existing_version);
}

void IndexedDBCallbacksWrapper::OnUpgradeNeeded(
    int64 old_version,
    scoped_refptr<IndexedDBDatabase> database,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(callbacks_);
  did_create_proxy_ = true;
  callbacks_->onUpgradeNeeded(
      old_version,
      new WebIDBDatabaseImpl(database, database_callbacks_),
      metadata);
  database_callbacks_ = NULL;
}

void IndexedDBCallbacksWrapper::OnSuccess(
    scoped_refptr<IndexedDBDatabase> database,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK(callbacks_);
  scoped_refptr<IndexedDBCallbacksWrapper> self(this);

  WebIDBDatabaseImpl* impl =
      did_create_proxy_ ? 0
                        : new WebIDBDatabaseImpl(database, database_callbacks_);
  database_callbacks_ = NULL;

  callbacks_->onSuccess(impl, metadata);
  callbacks_.reset();
}

void IndexedDBCallbacksWrapper::SetDatabaseCallbacks(
    scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks) {
  database_callbacks_ = database_callbacks;
}
}  // namespace content

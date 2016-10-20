// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_callbacks_impl.h"

#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBMetadata.h"

using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBMetadata;
using blink::WebString;
using blink::WebVector;

namespace content {

namespace {

void ConvertIndexMetadata(const content::IndexedDBIndexMetadata& metadata,
                          WebIDBMetadata::Index* output) {
  output->id = metadata.id;
  output->name = metadata.name;
  output->keyPath = WebIDBKeyPathBuilder::Build(metadata.key_path);
  output->unique = metadata.unique;
  output->multiEntry = metadata.multi_entry;
}

void ConvertObjectStoreMetadata(
    const content::IndexedDBObjectStoreMetadata& metadata,
    WebIDBMetadata::ObjectStore* output) {
  output->id = metadata.id;
  output->name = metadata.name;
  output->keyPath = WebIDBKeyPathBuilder::Build(metadata.key_path);
  output->autoIncrement = metadata.auto_increment;
  output->maxIndexId = metadata.max_index_id;
  output->indexes = WebVector<WebIDBMetadata::Index>(metadata.indexes.size());
  size_t i = 0;
  for (const auto& iter : metadata.indexes)
    ConvertIndexMetadata(iter.second, &output->indexes[i++]);
}

void ConvertDatabaseMetadata(const content::IndexedDBDatabaseMetadata& metadata,
                             WebIDBMetadata* output) {
  output->id = metadata.id;
  output->name = metadata.name;
  output->version = metadata.version;
  output->maxObjectStoreId = metadata.max_object_store_id;
  output->objectStores =
      WebVector<WebIDBMetadata::ObjectStore>(metadata.object_stores.size());
  size_t i = 0;
  for (const auto& iter : metadata.object_stores)
    ConvertObjectStoreMetadata(iter.second, &output->objectStores[i++]);
}

}  // namespace

IndexedDBCallbacksImpl::IndexedDBCallbacksImpl(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    scoped_refptr<ThreadSafeSender> thread_safe_sender)
    : internal_state_(new InternalState(std::move(callbacks),
                                        std::move(thread_safe_sender))),
      callback_runner_(base::ThreadTaskRunnerHandle::Get()) {}

IndexedDBCallbacksImpl::~IndexedDBCallbacksImpl() {
  callback_runner_->DeleteSoon(FROM_HERE, internal_state_);
}

void IndexedDBCallbacksImpl::Error(int32_t code,
                                   const base::string16& message) {
  callback_runner_->PostTask(
      FROM_HERE, base::Bind(&InternalState::Error,
                            base::Unretained(internal_state_), code, message));
}

void IndexedDBCallbacksImpl::SuccessStringList(
    const std::vector<base::string16>& value) {
  callback_runner_->PostTask(
      FROM_HERE, base::Bind(&InternalState::SuccessStringList,
                            base::Unretained(internal_state_), value));
}

void IndexedDBCallbacksImpl::Blocked(int64_t existing_version) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InternalState::Blocked, base::Unretained(internal_state_),
                 existing_version));
}

void IndexedDBCallbacksImpl::UpgradeNeeded(
    int32_t database_id,
    int64_t old_version,
    blink::WebIDBDataLoss data_loss,
    const std::string& data_loss_message,
    const content::IndexedDBDatabaseMetadata& metadata) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InternalState::UpgradeNeeded,
                 base::Unretained(internal_state_), database_id, old_version,
                 data_loss, data_loss_message, metadata));
}

void IndexedDBCallbacksImpl::SuccessDatabase(
    int32_t database_id,
    const content::IndexedDBDatabaseMetadata& metadata) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InternalState::SuccessDatabase,
                 base::Unretained(internal_state_), database_id, metadata));
}

void IndexedDBCallbacksImpl::SuccessInteger(int64_t value) {
  callback_runner_->PostTask(
      FROM_HERE, base::Bind(&InternalState::SuccessInteger,
                            base::Unretained(internal_state_), value));
}

IndexedDBCallbacksImpl::InternalState::InternalState(
    std::unique_ptr<blink::WebIDBCallbacks> callbacks,
    scoped_refptr<ThreadSafeSender> thread_safe_sender)
    : callbacks_(std::move(callbacks)),
      thread_safe_sender_(std::move(thread_safe_sender)) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RegisterMojoOwnedCallbacks(this);
}

IndexedDBCallbacksImpl::InternalState::~InternalState() {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->UnregisterMojoOwnedCallbacks(this);
}

void IndexedDBCallbacksImpl::InternalState::Error(
    int32_t code,
    const base::string16& message) {
  callbacks_->onError(blink::WebIDBDatabaseError(code, message));
}

void IndexedDBCallbacksImpl::InternalState::SuccessStringList(
    const std::vector<base::string16>& value) {
  callbacks_->onSuccess(WebVector<WebString>(value));
}

void IndexedDBCallbacksImpl::InternalState::Blocked(int64_t existing_version) {
  callbacks_->onBlocked(existing_version);
}

void IndexedDBCallbacksImpl::InternalState::UpgradeNeeded(
    int32_t database_id,
    int64_t old_version,
    blink::WebIDBDataLoss data_loss,
    const std::string& data_loss_message,
    const content::IndexedDBDatabaseMetadata& metadata) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  WebIDBDatabase* database = dispatcher->RegisterDatabase(database_id);
  WebIDBMetadata web_metadata;
  ConvertDatabaseMetadata(metadata, &web_metadata);
  callbacks_->onUpgradeNeeded(old_version, database, web_metadata, data_loss,
                              WebString::fromUTF8(data_loss_message));
}

void IndexedDBCallbacksImpl::InternalState::SuccessDatabase(
    int32_t database_id,
    const content::IndexedDBDatabaseMetadata& metadata) {
  WebIDBDatabase* database = nullptr;
  if (database_id != kNoDatabase) {
    IndexedDBDispatcher* dispatcher =
        IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
    database = dispatcher->RegisterDatabase(database_id);
  }
  WebIDBMetadata web_metadata;
  ConvertDatabaseMetadata(metadata, &web_metadata);
  callbacks_->onSuccess(database, web_metadata);
}

void IndexedDBCallbacksImpl::InternalState::SuccessInteger(int64_t value) {
  callbacks_->onSuccess(value);
}

}  // namespace content

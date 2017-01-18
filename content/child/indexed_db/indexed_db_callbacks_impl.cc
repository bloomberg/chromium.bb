// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_callbacks_impl.h"

#include "base/threading/thread_task_runner_handle.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/indexed_db/webidbcursor_impl.h"
#include "content/child/indexed_db/webidbdatabase_impl.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBMetadata.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBValue.h"

using blink::WebBlobInfo;
using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBMetadata;
using blink::WebIDBValue;
using blink::WebString;
using blink::WebVector;
using indexed_db::mojom::DatabaseAssociatedPtrInfo;

namespace content {

namespace {

void ConvertIndexMetadata(const content::IndexedDBIndexMetadata& metadata,
                          WebIDBMetadata::Index* output) {
  output->id = metadata.id;
  output->name = WebString::fromUTF16(metadata.name);
  output->keyPath = WebIDBKeyPathBuilder::Build(metadata.key_path);
  output->unique = metadata.unique;
  output->multiEntry = metadata.multi_entry;
}

void ConvertObjectStoreMetadata(
    const content::IndexedDBObjectStoreMetadata& metadata,
    WebIDBMetadata::ObjectStore* output) {
  output->id = metadata.id;
  output->name = WebString::fromUTF16(metadata.name);
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
  output->name = WebString::fromUTF16(metadata.name);
  output->version = metadata.version;
  output->maxObjectStoreId = metadata.max_object_store_id;
  output->objectStores =
      WebVector<WebIDBMetadata::ObjectStore>(metadata.object_stores.size());
  size_t i = 0;
  for (const auto& iter : metadata.object_stores)
    ConvertObjectStoreMetadata(iter.second, &output->objectStores[i++]);
}

void ConvertReturnValue(const indexed_db::mojom::ReturnValuePtr& value,
                        WebIDBValue* web_value) {
  IndexedDBCallbacksImpl::ConvertValue(value->value, web_value);
  web_value->primaryKey = WebIDBKeyBuilder::Build(value->primary_key);
  web_value->keyPath = WebIDBKeyPathBuilder::Build(value->key_path);
}

}  // namespace

// static
void IndexedDBCallbacksImpl::ConvertValue(
    const indexed_db::mojom::ValuePtr& value,
    WebIDBValue* web_value) {
  if (value->bits.empty())
    return;

  blink::WebVector<WebBlobInfo> local_blob_info(
      value->blob_or_file_info.size());
  for (size_t i = 0; i < value->blob_or_file_info.size(); ++i) {
    const auto& info = value->blob_or_file_info[i];
    if (info->file) {
      local_blob_info[i] =
          WebBlobInfo(WebString::fromUTF8(info->uuid),
                      blink::FilePathToWebString(info->file->path),
                      WebString::fromUTF16(info->file->name),
                      WebString::fromUTF16(info->mime_type),
                      info->file->last_modified.ToDoubleT(), info->size);
    } else {
      local_blob_info[i] =
          WebBlobInfo(WebString::fromUTF8(info->uuid),
                      WebString::fromUTF16(info->mime_type), info->size);
    }
  }

  web_value->data.assign(&*value->bits.begin(), value->bits.size());
  web_value->webBlobInfo.swap(local_blob_info);
}


IndexedDBCallbacksImpl::IndexedDBCallbacksImpl(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    int64_t transaction_id,
    const base::WeakPtr<WebIDBCursorImpl>& cursor,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner)
    : internal_state_(new InternalState(std::move(callbacks),
                                        transaction_id,
                                        cursor,
                                        std::move(io_runner))),
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
    DatabaseAssociatedPtrInfo database,
    int64_t old_version,
    blink::WebIDBDataLoss data_loss,
    const std::string& data_loss_message,
    const content::IndexedDBDatabaseMetadata& metadata) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InternalState::UpgradeNeeded,
                 base::Unretained(internal_state_), base::Passed(&database),
                 old_version, data_loss, data_loss_message, metadata));
}

void IndexedDBCallbacksImpl::SuccessDatabase(
    DatabaseAssociatedPtrInfo database,
    const content::IndexedDBDatabaseMetadata& metadata) {
  callback_runner_->PostTask(FROM_HERE,
                             base::Bind(&InternalState::SuccessDatabase,
                                        base::Unretained(internal_state_),
                                        base::Passed(&database), metadata));
}

void IndexedDBCallbacksImpl::SuccessCursor(
    indexed_db::mojom::CursorAssociatedPtrInfo cursor,
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    indexed_db::mojom::ValuePtr value) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InternalState::SuccessCursor,
                 base::Unretained(internal_state_), base::Passed(&cursor), key,
                 primary_key, base::Passed(&value)));
}

void IndexedDBCallbacksImpl::SuccessValue(
    indexed_db::mojom::ReturnValuePtr value) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InternalState::SuccessValue,
                 base::Unretained(internal_state_), base::Passed(&value)));
}

void IndexedDBCallbacksImpl::SuccessCursorContinue(
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    indexed_db::mojom::ValuePtr value) {
  callback_runner_->PostTask(
      FROM_HERE, base::Bind(&InternalState::SuccessCursorContinue,
                            base::Unretained(internal_state_), key, primary_key,
                            base::Passed(&value)));
}

void IndexedDBCallbacksImpl::SuccessCursorPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    std::vector<indexed_db::mojom::ValuePtr> values) {
  callback_runner_->PostTask(FROM_HERE,
                             base::Bind(&InternalState::SuccessCursorPrefetch,
                                        base::Unretained(internal_state_), keys,
                                        primary_keys, base::Passed(&values)));
}

void IndexedDBCallbacksImpl::SuccessArray(
    std::vector<indexed_db::mojom::ReturnValuePtr> values) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InternalState::SuccessArray,
                 base::Unretained(internal_state_), base::Passed(&values)));
}

void IndexedDBCallbacksImpl::SuccessKey(const IndexedDBKey& key) {
  callback_runner_->PostTask(
      FROM_HERE, base::Bind(&InternalState::SuccessKey,
                            base::Unretained(internal_state_), key));
}

void IndexedDBCallbacksImpl::SuccessInteger(int64_t value) {
  callback_runner_->PostTask(
      FROM_HERE, base::Bind(&InternalState::SuccessInteger,
                            base::Unretained(internal_state_), value));
}

void IndexedDBCallbacksImpl::Success() {
  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InternalState::Success, base::Unretained(internal_state_)));
}

IndexedDBCallbacksImpl::InternalState::InternalState(
    std::unique_ptr<blink::WebIDBCallbacks> callbacks,
    int64_t transaction_id,
    const base::WeakPtr<WebIDBCursorImpl>& cursor,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner)
    : callbacks_(std::move(callbacks)),
      transaction_id_(transaction_id),
      cursor_(cursor),
      io_runner_(std::move(io_runner)) {
  IndexedDBDispatcher::ThreadSpecificInstance()->RegisterMojoOwnedCallbacks(
      this);
}

IndexedDBCallbacksImpl::InternalState::~InternalState() {
  IndexedDBDispatcher::ThreadSpecificInstance()->UnregisterMojoOwnedCallbacks(
      this);
}

void IndexedDBCallbacksImpl::InternalState::Error(
    int32_t code,
    const base::string16& message) {
  callbacks_->onError(
      blink::WebIDBDatabaseError(code, WebString::fromUTF16(message)));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::SuccessStringList(
    const std::vector<base::string16>& value) {
  WebVector<WebString> web_value(value.size());
  std::transform(
      value.begin(), value.end(), web_value.begin(),
      [](const base::string16& s) { return WebString::fromUTF16(s); });
  callbacks_->onSuccess(web_value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::Blocked(int64_t existing_version) {
  callbacks_->onBlocked(existing_version);
  // Not resetting |callbacks_|.
}

void IndexedDBCallbacksImpl::InternalState::UpgradeNeeded(
    DatabaseAssociatedPtrInfo database_info,
    int64_t old_version,
    blink::WebIDBDataLoss data_loss,
    const std::string& data_loss_message,
    const content::IndexedDBDatabaseMetadata& metadata) {
  WebIDBDatabase* database =
      new WebIDBDatabaseImpl(std::move(database_info), io_runner_);
  WebIDBMetadata web_metadata;
  ConvertDatabaseMetadata(metadata, &web_metadata);
  callbacks_->onUpgradeNeeded(old_version, database, web_metadata, data_loss,
                              WebString::fromUTF8(data_loss_message));
  // Not resetting |callbacks_|.
}

void IndexedDBCallbacksImpl::InternalState::SuccessDatabase(
    DatabaseAssociatedPtrInfo database_info,
    const content::IndexedDBDatabaseMetadata& metadata) {
  WebIDBDatabase* database = nullptr;
  if (database_info.is_valid())
    database = new WebIDBDatabaseImpl(std::move(database_info), io_runner_);

  WebIDBMetadata web_metadata;
  ConvertDatabaseMetadata(metadata, &web_metadata);
  callbacks_->onSuccess(database, web_metadata);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::SuccessCursor(
    indexed_db::mojom::CursorAssociatedPtrInfo cursor_info,
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    indexed_db::mojom::ValuePtr value) {
  WebIDBValue web_value;
  if (value)
    IndexedDBCallbacksImpl::ConvertValue(value, &web_value);

  WebIDBCursorImpl* cursor =
      new WebIDBCursorImpl(std::move(cursor_info), transaction_id_, io_runner_);
  callbacks_->onSuccess(cursor, WebIDBKeyBuilder::Build(key),
                        WebIDBKeyBuilder::Build(primary_key), web_value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::SuccessKey(
    const IndexedDBKey& key) {
  callbacks_->onSuccess(WebIDBKeyBuilder::Build(key));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::SuccessValue(
    indexed_db::mojom::ReturnValuePtr value) {
  WebIDBValue web_value;
  if (value)
    ConvertReturnValue(value, &web_value);
  callbacks_->onSuccess(web_value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::SuccessCursorContinue(
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    indexed_db::mojom::ValuePtr value) {
  WebIDBValue web_value;
  if (value)
    ConvertValue(value, &web_value);
  callbacks_->onSuccess(WebIDBKeyBuilder::Build(key),
                        WebIDBKeyBuilder::Build(primary_key), web_value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::SuccessCursorPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    std::vector<indexed_db::mojom::ValuePtr> values) {
  std::vector<WebIDBValue> web_values(values.size());
  for (size_t i = 0; i < values.size(); ++i)
    ConvertValue(values[i], &web_values[i]);

  if (cursor_) {
    cursor_->SetPrefetchData(keys, primary_keys, web_values);
    cursor_->CachedContinue(callbacks_.get());
  }
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::SuccessArray(
    std::vector<indexed_db::mojom::ReturnValuePtr> values) {
  blink::WebVector<WebIDBValue> web_values(values.size());
  for (size_t i = 0; i < values.size(); ++i)
    ConvertReturnValue(values[i], &web_values[i]);
  callbacks_->onSuccess(web_values);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::SuccessInteger(int64_t value) {
  callbacks_->onSuccess(value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::InternalState::Success() {
  callbacks_->onSuccess();
  callbacks_.reset();
}

}  // namespace content

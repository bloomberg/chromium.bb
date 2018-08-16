// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/indexed_db/webidbdatabase_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "content/renderer/indexed_db/indexed_db_callbacks_impl.h"
#include "content/renderer/indexed_db/indexed_db_dispatcher.h"
#include "content/renderer/indexed_db/indexed_db_key_builders.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key_path.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_metadata.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

using blink::IndexedDBKey;
using blink::IndexedDBIndexKeys;
using blink::WebBlobInfo;
using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseCallbacks;
using blink::WebIDBMetadata;
using blink::WebIDBKey;
using blink::WebIDBKeyPath;
using blink::WebIDBKeyRange;
using blink::WebIDBKeyView;
using blink::WebString;
using blink::WebVector;
using indexed_db::mojom::CallbacksAssociatedPtrInfo;
using indexed_db::mojom::DatabaseAssociatedPtrInfo;

namespace content {

namespace {

std::vector<IndexedDBIndexKeys> ConvertWebIndexKeys(
    const WebVector<long long>& index_ids,
    const WebVector<WebIDBDatabase::WebIndexKeys>& index_keys) {
  DCHECK_EQ(index_ids.size(), index_keys.size());
  std::vector<IndexedDBIndexKeys> result;
  result.reserve(index_ids.size());
  for (size_t i = 0, len = index_ids.size(); i < len; ++i) {
    result.emplace_back(index_ids[i], std::vector<IndexedDBKey>());
    std::vector<IndexedDBKey>& result_keys = result.back().second;
    result_keys.reserve(index_keys[i].size());
    for (const WebIDBKey& index_key : index_keys[i])
      result_keys.emplace_back(IndexedDBKeyBuilder::Build(index_key.View()));
  }
  return result;
}

}  // namespace

WebIDBDatabaseImpl::WebIDBDatabaseImpl(DatabaseAssociatedPtrInfo database_info)
    : database_(std::move(database_info)) {}

WebIDBDatabaseImpl::~WebIDBDatabaseImpl() = default;

void WebIDBDatabaseImpl::CreateObjectStore(long long transaction_id,
                                           long long object_store_id,
                                           const WebString& name,
                                           const WebIDBKeyPath& key_path,
                                           bool auto_increment) {
  database_->CreateObjectStore(transaction_id, object_store_id, name.Utf16(),
                               IndexedDBKeyPathBuilder::Build(key_path),
                               auto_increment);
}

void WebIDBDatabaseImpl::DeleteObjectStore(long long transaction_id,
                                           long long object_store_id) {
  database_->DeleteObjectStore(transaction_id, object_store_id);
}

void WebIDBDatabaseImpl::RenameObjectStore(long long transaction_id,
                                           long long object_store_id,
                                           const blink::WebString& new_name) {
  database_->RenameObjectStore(transaction_id, object_store_id,
                               new_name.Utf16());
}

void WebIDBDatabaseImpl::CreateTransaction(
    long long transaction_id,
    const WebVector<long long>& object_store_ids,
    blink::WebIDBTransactionMode mode) {
  database_->CreateTransaction(
      transaction_id,
      std::vector<int64_t>(object_store_ids.begin(), object_store_ids.end()),
      mode);
}

void WebIDBDatabaseImpl::Close() {
  database_->Close();
}

void WebIDBDatabaseImpl::VersionChangeIgnored() {
  database_->VersionChangeIgnored();
}

void WebIDBDatabaseImpl::AddObserver(
    long long transaction_id,
    int32_t observer_id,
    bool include_transaction,
    bool no_records,
    bool values,
    const std::bitset<blink::kWebIDBOperationTypeCount>& operation_types) {
  static_assert(blink::kWebIDBOperationTypeCount < sizeof(uint16_t) * CHAR_BIT,
                "WebIDBOperationType Count exceeds size of uint16_t");
  database_->AddObserver(transaction_id, observer_id, include_transaction,
                         no_records, values, operation_types.to_ulong());
}

void WebIDBDatabaseImpl::RemoveObservers(
    const WebVector<int32_t>& observer_ids_to_remove) {
  std::vector<int32_t> remove_observer_ids(
      observer_ids_to_remove.Data(),
      observer_ids_to_remove.Data() + observer_ids_to_remove.size());
  database_->RemoveObservers(remove_observer_ids);
}

void WebIDBDatabaseImpl::Get(long long transaction_id,
                             long long object_store_id,
                             long long index_id,
                             const WebIDBKeyRange& key_range,
                             bool key_only,
                             WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ThreadSpecificInstance()->ResetCursorPrefetchCaches(
      transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->Get(transaction_id, object_store_id, index_id,
                 IndexedDBKeyRangeBuilder::Build(key_range), key_only,
                 GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::GetAll(long long transaction_id,
                                long long object_store_id,
                                long long index_id,
                                const WebIDBKeyRange& key_range,
                                long long max_count,
                                bool key_only,
                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ThreadSpecificInstance()->ResetCursorPrefetchCaches(
      transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->GetAll(transaction_id, object_store_id, index_id,
                    IndexedDBKeyRangeBuilder::Build(key_range), key_only,
                    max_count, GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::Put(long long transaction_id,
                             long long object_store_id,
                             const blink::WebData& value,
                             const WebVector<WebBlobInfo>& web_blob_info,
                             WebIDBKeyView web_primary_key,
                             blink::WebIDBPutMode put_mode,
                             WebIDBCallbacks* callbacks,
                             const WebVector<long long>& index_ids,
                             WebVector<WebIndexKeys> index_keys) {
  IndexedDBKey key = IndexedDBKeyBuilder::Build(web_primary_key);

  if (value.size() + key.size_estimate() > max_put_value_size_) {
    callbacks->OnError(blink::WebIDBDatabaseError(
        blink::kWebIDBDatabaseExceptionUnknownError,
        WebString::FromUTF8(base::StringPrintf(
            "The serialized value is too large"
            " (size=%" PRIuS " bytes, max=%" PRIuS " bytes).",
            value.size(), max_put_value_size_))));
    return;
  }

  IndexedDBDispatcher::ThreadSpecificInstance()->ResetCursorPrefetchCaches(
      transaction_id, nullptr);

  auto mojo_value = blink::mojom::IDBValue::New();
  DCHECK(mojo_value->bits.empty());
  mojo_value->bits.reserve(value.size());
  value.ForEachSegment([&mojo_value](const char* segment, size_t segment_size,
                                     size_t segment_offset) {
    mojo_value->bits.append(segment, segment_size);
    return true;
  });
  mojo_value->blob_or_file_info.reserve(web_blob_info.size());
  for (const WebBlobInfo& info : web_blob_info) {
    auto blob_info = blink::mojom::IDBBlobInfo::New();
    if (info.IsFile()) {
      blob_info->file = blink::mojom::IDBFileInfo::New();
      blob_info->file->path = blink::WebStringToFilePath(info.FilePath());
      blob_info->file->name = info.FileName().Utf16();
      blob_info->file->last_modified =
          base::Time::FromDoubleT(info.LastModified());
    }
    blob_info->size = info.size();
    blob_info->uuid = info.Uuid().Latin1();
    DCHECK(blob_info->uuid.size());
    blob_info->mime_type = info.GetType().Utf16();
    blob_info->blob = blink::mojom::BlobPtrInfo(info.CloneBlobHandle(),
                                                blink::mojom::Blob::Version_);
    mojo_value->blob_or_file_info.push_back(std::move(blob_info));
  }

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->Put(transaction_id, object_store_id, std::move(mojo_value), key,
                 put_mode, ConvertWebIndexKeys(index_ids, index_keys),
                 GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::SetIndexKeys(
    long long transaction_id,
    long long object_store_id,
    WebIDBKeyView primary_key,
    const WebVector<long long>& index_ids,
    const WebVector<WebIndexKeys>& index_keys) {
  database_->SetIndexKeys(transaction_id, object_store_id,
                          IndexedDBKeyBuilder::Build(primary_key),
                          ConvertWebIndexKeys(index_ids, index_keys));
}

void WebIDBDatabaseImpl::SetIndexesReady(
    long long transaction_id,
    long long object_store_id,
    const WebVector<long long>& web_index_ids) {
  std::vector<int64_t> index_ids(web_index_ids.Data(),
                                 web_index_ids.Data() + web_index_ids.size());
  database_->SetIndexesReady(transaction_id, object_store_id,
                             std::move(index_ids));
}

void WebIDBDatabaseImpl::OpenCursor(long long transaction_id,
                                    long long object_store_id,
                                    long long index_id,
                                    const WebIDBKeyRange& key_range,
                                    blink::WebIDBCursorDirection direction,
                                    bool key_only,
                                    blink::WebIDBTaskType task_type,
                                    WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ThreadSpecificInstance()->ResetCursorPrefetchCaches(
      transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->OpenCursor(transaction_id, object_store_id, index_id,
                        IndexedDBKeyRangeBuilder::Build(key_range), direction,
                        key_only, task_type,
                        GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::Count(long long transaction_id,
                               long long object_store_id,
                               long long index_id,
                               const WebIDBKeyRange& key_range,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ThreadSpecificInstance()->ResetCursorPrefetchCaches(
      transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->Count(transaction_id, object_store_id, index_id,
                   IndexedDBKeyRangeBuilder::Build(key_range),
                   GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::Delete(long long transaction_id,
                                long long object_store_id,
                                WebIDBKeyView primary_key,
                                WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ThreadSpecificInstance()->ResetCursorPrefetchCaches(
      transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->DeleteRange(transaction_id, object_store_id,
                         IndexedDBKeyRangeBuilder::Build(primary_key),
                         GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::DeleteRange(long long transaction_id,
                                     long long object_store_id,
                                     const WebIDBKeyRange& key_range,
                                     WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ThreadSpecificInstance()->ResetCursorPrefetchCaches(
      transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->DeleteRange(transaction_id, object_store_id,
                         IndexedDBKeyRangeBuilder::Build(key_range),
                         GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::Clear(long long transaction_id,
                               long long object_store_id,
                               WebIDBCallbacks* callbacks) {
  IndexedDBDispatcher::ThreadSpecificInstance()->ResetCursorPrefetchCaches(
      transaction_id, nullptr);

  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr);
  database_->Clear(transaction_id, object_store_id,
                   GetCallbacksProxy(std::move(callbacks_impl)));
}

void WebIDBDatabaseImpl::CreateIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id,
                                     const WebString& name,
                                     const WebIDBKeyPath& key_path,
                                     bool unique,
                                     bool multi_entry) {
  database_->CreateIndex(transaction_id, object_store_id, index_id,
                         name.Utf16(), IndexedDBKeyPathBuilder::Build(key_path),
                         unique, multi_entry);
}

void WebIDBDatabaseImpl::DeleteIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id) {
  database_->DeleteIndex(transaction_id, object_store_id, index_id);
}

void WebIDBDatabaseImpl::RenameIndex(long long transaction_id,
                                     long long object_store_id,
                                     long long index_id,
                                     const WebString& new_name) {
  database_->RenameIndex(transaction_id, object_store_id, index_id,
                         new_name.Utf16());
}

void WebIDBDatabaseImpl::Abort(long long transaction_id) {
  database_->Abort(transaction_id);
}

void WebIDBDatabaseImpl::Commit(long long transaction_id) {
  database_->Commit(transaction_id);
}

CallbacksAssociatedPtrInfo WebIDBDatabaseImpl::GetCallbacksProxy(
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  CallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

}  // namespace content

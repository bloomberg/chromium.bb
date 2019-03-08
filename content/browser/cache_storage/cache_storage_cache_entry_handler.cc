// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache_entry_handler.h"

#include "base/guid.h"
#include "base/optional.h"
#include "content/browser/background_fetch/storage/cache_entry_handler_impl.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace content {

CacheStorageCacheEntryHandler::BlobDataHandle::BlobDataHandle(
    base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler,
    CacheStorageCacheHandle cache_handle,
    disk_cache::ScopedEntryPtr entry)
    : entry_handler_(std::move(entry_handler)),
      cache_handle_(std::move(cache_handle)),
      entry_(std::move(entry)) {}

bool CacheStorageCacheEntryHandler::BlobDataHandle::IsValid() {
  return entry_ != nullptr;
}

void CacheStorageCacheEntryHandler::BlobDataHandle::Invalidate() {
  cache_handle_ = base::nullopt;
  entry_handler_ = nullptr;
  entry_ = nullptr;
}

CacheStorageCacheEntryHandler::BlobDataHandle::~BlobDataHandle() {
  if (entry_handler_)
    entry_handler_->EraseBlobDataHandle(this);
}

PutContext::PutContext(blink::mojom::FetchAPIRequestPtr request,
                       blink::mojom::FetchAPIResponsePtr response,
                       blink::mojom::BlobPtr blob,
                       uint64_t blob_size,
                       blink::mojom::BlobPtr side_data_blob,
                       uint64_t side_data_blob_size,
                       int64_t trace_id)
    : request(std::move(request)),
      response(std::move(response)),
      blob(std::move(blob)),
      blob_size(blob_size),
      side_data_blob(std::move(side_data_blob)),
      side_data_blob_size(side_data_blob_size),
      trace_id(trace_id) {}

PutContext::~PutContext() = default;

// Default implemetation of CacheStorageCacheEntryHandler.
class CacheStorageCacheEntryHandlerImpl : public CacheStorageCacheEntryHandler {
 public:
  CacheStorageCacheEntryHandlerImpl(
      base::WeakPtr<storage::BlobStorageContext> blob_context)
      : CacheStorageCacheEntryHandler(std::move(blob_context)),
        weak_ptr_factory_(this) {}
  ~CacheStorageCacheEntryHandlerImpl() override = default;

  std::unique_ptr<PutContext> CreatePutContext(
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchAPIResponsePtr response,
      int64_t trace_id) override {
    blink::mojom::BlobPtr blob;
    uint64_t blob_size = blink::BlobUtils::kUnknownSize;
    blink::mojom::BlobPtr side_data_blob;
    uint64_t side_data_blob_size = blink::BlobUtils::kUnknownSize;

    if (response->blob) {
      blob.Bind(std::move(response->blob->blob));
      blob_size = response->blob->size;
    }
    if (response->side_data_blob) {
      side_data_blob.Bind(std::move(response->side_data_blob->blob));
      side_data_blob_size = response->side_data_blob->size;
    }

    return std::make_unique<PutContext>(
        std::move(request), std::move(response), std::move(blob), blob_size,
        std::move(side_data_blob), side_data_blob_size, trace_id);
  }

  void PopulateResponseBody(scoped_refptr<BlobDataHandle> data_handle,
                            blink::mojom::FetchAPIResponse* response) override {
    disk_cache::Entry* entry = data_handle->entry().get();
    DCHECK(entry);

    // Create a blob with the response body data.
    response->blob = blink::mojom::SerializedBlob::New();
    response->blob->size =
        entry->GetDataSize(CacheStorageCache::INDEX_RESPONSE_BODY);
    response->blob->uuid = base::GenerateGUID();
    auto blob_data =
        std::make_unique<storage::BlobDataBuilder>(response->blob->uuid);

    blob_data->AppendDiskCacheEntryWithSideData(
        std::move(data_handle), entry, CacheStorageCache::INDEX_RESPONSE_BODY,
        CacheStorageCache::INDEX_SIDE_DATA);
    auto blob_handle = blob_context_->AddFinishedBlob(std::move(blob_data));

    storage::BlobImpl::Create(std::move(blob_handle),
                              MakeRequest(&response->blob->blob));
  }

  void PopulateRequestBody(scoped_refptr<BlobDataHandle> data_handle,
                           blink::mojom::FetchAPIRequest* request) override {}

 private:
  base::WeakPtr<CacheStorageCacheEntryHandler> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<CacheStorageCacheEntryHandlerImpl> weak_ptr_factory_;
};

CacheStorageCacheEntryHandler::CacheStorageCacheEntryHandler(
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : blob_context_(blob_context) {}

scoped_refptr<CacheStorageCacheEntryHandler::BlobDataHandle>
CacheStorageCacheEntryHandler::CreateBlobDataHandle(
    CacheStorageCacheHandle cache_handle,
    disk_cache::ScopedEntryPtr entry) {
  auto handle =
      base::MakeRefCounted<CacheStorageCacheEntryHandler::BlobDataHandle>(
          GetWeakPtr(), std::move(cache_handle), std::move(entry));
  DCHECK_EQ(blob_data_handles_.count(handle.get()), 0u);
  blob_data_handles_.insert(handle.get());
  return handle;
}

CacheStorageCacheEntryHandler::~CacheStorageCacheEntryHandler() = default;

void CacheStorageCacheEntryHandler::InvalidateBlobDataHandles() {
  // Calling Invalidate() can cause the CacheStorageCacheEntryHandler to be
  // destroyed. Be careful not to touch |this| after calling Invalidate().
  std::set<BlobDataHandle*> handles = std::move(blob_data_handles_);
  for (auto* handle : handles)
    handle->Invalidate();
}

void CacheStorageCacheEntryHandler::EraseBlobDataHandle(
    BlobDataHandle* handle) {
  DCHECK_NE(blob_data_handles_.count(handle), 0u);
  blob_data_handles_.erase(handle);
}

// static
std::unique_ptr<CacheStorageCacheEntryHandler>
CacheStorageCacheEntryHandler::CreateCacheEntryHandler(
    CacheStorageOwner owner,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  switch (owner) {
    case CacheStorageOwner::kCacheAPI:
      return std::make_unique<CacheStorageCacheEntryHandlerImpl>(
          std::move(blob_context));
    case CacheStorageOwner::kBackgroundFetch:
      return std::make_unique<background_fetch::CacheEntryHandlerImpl>(
          std::move(blob_context));
  }
  NOTREACHED();
}

}  // namespace content

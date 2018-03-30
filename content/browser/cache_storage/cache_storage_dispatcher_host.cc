// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/WebKit/public/platform/modules/cache_storage/cache_storage.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"


namespace content {

namespace {

using blink::mojom::CacheStorageError;

const int32_t kCachePreservationSeconds = 5;

// TODO(lucmult): Check this before binding.
bool OriginCanAccessCacheStorage(const url::Origin& origin) {
  return !origin.unique() && IsOriginSecure(origin.GetURL());
}

void StopPreservingCache(CacheStorageCacheHandle cache_handle) {}

}  // namespace

// Implements the mojom interface CacheStorageCache. It's owned by
// CacheStorageDispatcherHost and it's destroyed when client drops the mojo ptr
// which in turn removes from StrongBindingSet in CacheStorageDispatcherHost.
class CacheStorageDispatcherHost::CacheImpl
    : public blink::mojom::CacheStorageCache {
 public:
  CacheImpl(CacheStorageCacheHandle cache_handle,
            CacheStorageDispatcherHost* dispatcher_host)
      : cache_handle_(std::move(cache_handle)),
        owner_(dispatcher_host),
        weak_factory_(this) {}

  ~CacheImpl() override = default;

  // blink::mojom::CacheStorageCache implementation:
  void Match(const ServiceWorkerFetchRequest& request,
             const CacheStorageCacheQueryParams& match_params,
             MatchCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    auto scoped_request = std::make_unique<ServiceWorkerFetchRequest>(
        request.url, request.method, request.headers, request.referrer,
        request.is_reload);
    cache->Match(std::move(scoped_request), match_params,
                 base::BindOnce(&CacheImpl::OnCacheMatchCallback,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                cache_handle_.Clone()));
  }

  void OnCacheMatchCallback(
      blink::mojom::CacheStorageCache::MatchCallback callback,
      CacheStorageCacheHandle cache_handle,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<ServiceWorkerResponse> response,
      std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
    if (error != CacheStorageError::kSuccess) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(error));
      return;
    }

    if (blob_data_handle)
      owner_->StoreBlobDataHandle(*blob_data_handle);

    std::move(callback).Run(blink::mojom::MatchResult::NewResponse(*response));
  }

  void MatchAll(const ServiceWorkerFetchRequest& request,
                const CacheStorageCacheQueryParams& match_params,
                MatchAllCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::MatchAllResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    if (request.url.is_empty()) {
      cache->MatchAll(
          nullptr, match_params,
          base::BindOnce(&CacheImpl::OnCacheMatchAllCallback,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         cache_handle_.Clone()));
      return;
    }

    auto scoped_request = std::make_unique<ServiceWorkerFetchRequest>(
        request.url, request.method, request.headers, request.referrer,
        request.is_reload);
    if (match_params.ignore_search) {
      cache->MatchAll(
          std::move(scoped_request), match_params,
          base::BindOnce(&CacheImpl::OnCacheMatchAllCallback,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         cache_handle_.Clone()));
      return;
    }

    cache->Match(std::move(scoped_request), match_params,
                 base::BindOnce(&CacheImpl::OnCacheMatchAllCallbackAdapter,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                cache_handle_.Clone()));
  }

  void OnCacheMatchAllCallback(
      blink::mojom::CacheStorageCache::MatchAllCallback callback,
      CacheStorageCacheHandle cache_handle,
      blink::mojom::CacheStorageError error,
      std::vector<ServiceWorkerResponse> responses,
      std::unique_ptr<content::CacheStorageCache::BlobDataHandles>
          blob_data_handles) {
    if (error != CacheStorageError::kSuccess &&
        error != CacheStorageError::kErrorNotFound) {
      std::move(callback).Run(blink::mojom::MatchAllResult::NewStatus(error));
      return;
    }

    for (const auto& handle : *blob_data_handles) {
      if (handle)
        owner_->StoreBlobDataHandle(*handle);
    }

    std::move(callback).Run(
        blink::mojom::MatchAllResult::NewResponses(std::move(responses)));
  }

  void OnCacheMatchAllCallbackAdapter(
      blink::mojom::CacheStorageCache::MatchAllCallback callback,
      CacheStorageCacheHandle cache_handle,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<ServiceWorkerResponse> response,
      std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
    std::vector<ServiceWorkerResponse> responses;
    auto blob_data_handles =
        std::make_unique<content::CacheStorageCache::BlobDataHandles>();
    if (error == CacheStorageError::kSuccess) {
      DCHECK(response);
      responses.push_back(std::move(*response.release()));
      if (blob_data_handle)
        blob_data_handles->push_back(std::move(blob_data_handle));
    }
    OnCacheMatchAllCallback(std::move(callback), std::move(cache_handle), error,
                            std::move(responses), std::move(blob_data_handles));
  }

  void Keys(const ServiceWorkerFetchRequest& request,
            const CacheStorageCacheQueryParams& match_params,
            KeysCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::CacheKeysResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    auto request_ptr = std::make_unique<ServiceWorkerFetchRequest>(
        request.url, request.method, request.headers, request.referrer,
        request.is_reload);
    cache->Keys(std::move(request_ptr), match_params,
                base::BindOnce(&CacheImpl::OnCacheKeysCallback,
                               weak_factory_.GetWeakPtr(), std::move(callback),
                               cache_handle_.Clone()));
  }

  void OnCacheKeysCallback(
      blink::mojom::CacheStorageCache::KeysCallback callback,
      CacheStorageCacheHandle cache_handle,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<content::CacheStorageCache::Requests> requests) {
    if (error != CacheStorageError::kSuccess) {
      std::move(callback).Run(blink::mojom::CacheKeysResult::NewStatus(error));
      return;
    }

    std::move(callback).Run(blink::mojom::CacheKeysResult::NewKeys(*requests));
  }

  void Batch(const std::vector<CacheStorageBatchOperation>& batch_operations,
             BatchCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(CacheStorageError::kErrorNotFound);
      return;
    }
    cache->BatchOperation(
        batch_operations,
        base::BindOnce(&CacheImpl::OnCacheBatchCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       cache_handle_.Clone()),
        base::BindOnce(&CacheImpl::OnBadMessage, weak_factory_.GetWeakPtr(),
                       mojo::GetBadMessageCallback()));
  }

  void OnCacheBatchCallback(
      blink::mojom::CacheStorageCache::BatchCallback callback,
      CacheStorageCacheHandle cache_handle,
      blink::mojom::CacheStorageError error) {
    std::move(callback).Run(error);
  }

  void OnBadMessage(mojo::ReportBadMessageCallback bad_message_callback) {
    std::move(bad_message_callback).Run("CSDH_UNEXPECTED_OPERATION");
  }

  CacheStorageCacheHandle cache_handle_;

  // Owns this.
  CacheStorageDispatcherHost* const owner_;

  base::WeakPtrFactory<CacheImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacheImpl);
};

CacheStorageDispatcherHost::CacheStorageDispatcherHost() = default;

CacheStorageDispatcherHost::~CacheStorageDispatcherHost() = default;

void CacheStorageDispatcherHost::Init(CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&CacheStorageDispatcherHost::CreateCacheListener,
                     base::RetainedRef(this), base::RetainedRef(context)));
}

void CacheStorageDispatcherHost::CreateCacheListener(
    CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_ = context;
}

void CacheStorageDispatcherHost::Has(
    const base::string16& cache_name,
    blink::mojom::CacheStorage::HasCallback callback) {
  TRACE_EVENT0("CacheStorage", "CacheStorageDispatcherHost::OnCacheStorageHas");
  url::Origin origin = bindings_.dispatch_context();
  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  context_->cache_manager()->HasCache(
      origin, base::UTF16ToUTF8(cache_name),
      base::BindOnce(&CacheStorageDispatcherHost::OnHasCallback, this,
                     std::move(callback)));
}

void CacheStorageDispatcherHost::Open(
    const base::string16& cache_name,
    blink::mojom::CacheStorage::OpenCallback callback) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageOpen");
  url::Origin origin = bindings_.dispatch_context();
  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  context_->cache_manager()->OpenCache(
      origin, base::UTF16ToUTF8(cache_name),
      base::BindOnce(&CacheStorageDispatcherHost::OnOpenCallback, this, origin,
                     std::move(callback)));
}

void CacheStorageDispatcherHost::Delete(
    const base::string16& cache_name,
    blink::mojom::CacheStorage::DeleteCallback callback) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageDelete");
  url::Origin origin = bindings_.dispatch_context();
  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  context_->cache_manager()->DeleteCache(origin, base::UTF16ToUTF8(cache_name),
                                         std::move(callback));
}

void CacheStorageDispatcherHost::Keys(
    blink::mojom::CacheStorage::KeysCallback callback) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageKeys");
  url::Origin origin = bindings_.dispatch_context();

  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  context_->cache_manager()->EnumerateCaches(
      origin, base::BindOnce(&CacheStorageDispatcherHost::OnKeysCallback, this,
                             std::move(callback)));
}

void CacheStorageDispatcherHost::Match(
    const content::ServiceWorkerFetchRequest& request,
    const content::CacheStorageCacheQueryParams& match_params,
    blink::mojom::CacheStorage::MatchCallback callback) {
  TRACE_EVENT0("CacheStorage",
               "CacheStorageDispatcherHost::OnCacheStorageMatch");
  url::Origin origin = bindings_.dispatch_context();
  if (!OriginCanAccessCacheStorage(origin)) {
    bindings_.ReportBadMessage("CSDH_INVALID_ORIGIN");
    return;
  }
  if (!ValidState())
    return;
  auto scoped_request = std::make_unique<ServiceWorkerFetchRequest>(
      request.url, request.method, request.headers, request.referrer,
      request.is_reload);

  if (match_params.cache_name.is_null()) {
    context_->cache_manager()->MatchAllCaches(
        origin, std::move(scoped_request), std::move(match_params),
        base::BindOnce(&CacheStorageDispatcherHost::OnMatchCallback, this,
                       std::move(callback)));
    return;
  }
  context_->cache_manager()->MatchCache(
      origin, base::UTF16ToUTF8(match_params.cache_name.string()),
      std::move(scoped_request), std::move(match_params),
      base::BindOnce(&CacheStorageDispatcherHost::OnMatchCallback, this,
                     std::move(callback)));
}

void CacheStorageDispatcherHost::BlobDataHandled(const std::string& uuid) {
  DropBlobDataHandle(uuid);
}

void CacheStorageDispatcherHost::OnHasCallback(
    blink::mojom::CacheStorage::HasCallback callback,
    bool has_cache,
    CacheStorageError error) {
  if (!has_cache)
    error = CacheStorageError::kErrorNotFound;
  std::move(callback).Run(error);
}

void CacheStorageDispatcherHost::OnOpenCallback(
    url::Origin origin,
    blink::mojom::CacheStorage::OpenCallback callback,
    CacheStorageCacheHandle cache_handle,
    CacheStorageError error) {
  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(blink::mojom::OpenResult::NewStatus(error));
    return;
  }

  // Hang on to the cache for a few seconds. This way if the user quickly closes
  // and reopens it the cache backend won't have to be reinitialized.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&StopPreservingCache, cache_handle.Clone()),
      base::TimeDelta::FromSeconds(kCachePreservationSeconds));

  blink::mojom::CacheStorageCacheAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  auto cache_impl = std::make_unique<CacheImpl>(std::move(cache_handle), this);
  cache_bindings_.AddBinding(std::move(cache_impl), std::move(request));

  std::move(callback).Run(
      blink::mojom::OpenResult::NewCache(std::move(ptr_info)));
}

void CacheStorageDispatcherHost::OnKeysCallback(
    blink::mojom::CacheStorage::KeysCallback callback,
    const CacheStorageIndex& cache_index) {
  std::vector<base::string16> string16s;
  for (const auto& metadata : cache_index.ordered_cache_metadata()) {
    string16s.push_back(base::UTF8ToUTF16(metadata.name));
  }

  std::move(callback).Run(string16s);
}

void CacheStorageDispatcherHost::OnMatchCallback(
    blink::mojom::CacheStorage::MatchCallback callback,
    CacheStorageError error,
    std::unique_ptr<ServiceWorkerResponse> response,
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle) {
  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(blink::mojom::MatchResult::NewStatus(error));
    return;
  }

  if (blob_data_handle)
    StoreBlobDataHandle(*blob_data_handle);

  std::move(callback).Run(
      blink::mojom::MatchResult::NewResponse(std::move(*response)));
}

void CacheStorageDispatcherHost::StoreBlobDataHandle(
    const storage::BlobDataHandle& blob_data_handle) {
  std::pair<UUIDToBlobDataHandleList::iterator, bool> rv =
      blob_handle_store_.insert(std::make_pair(
          blob_data_handle.uuid(), std::list<storage::BlobDataHandle>()));
  rv.first->second.push_front(storage::BlobDataHandle(blob_data_handle));
}

void CacheStorageDispatcherHost::DropBlobDataHandle(const std::string& uuid) {
  UUIDToBlobDataHandleList::iterator it = blob_handle_store_.find(uuid);
  if (it == blob_handle_store_.end())
    return;
  DCHECK(!it->second.empty());
  it->second.pop_front();
  if (it->second.empty())
    blob_handle_store_.erase(it);
}

void CacheStorageDispatcherHost::AddBinding(
    blink::mojom::CacheStorageRequest request,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bindings_.AddBinding(this, std::move(request), origin);
}

bool CacheStorageDispatcherHost::ValidState() {
  // cache_manager() can return nullptr when process is shutting down.
  if (!(context_ && context_->cache_manager())) {
    bindings_.CloseAllBindings();
    return false;
  }
  return true;
}

}  // namespace content

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/referrer_type_converters.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseError;

// TODO(lucmult): Check this before binding.
bool OriginCanAccessCacheStorage(const url::Origin& origin) {
  return !origin.opaque() && IsOriginSecure(origin.GetURL());
}

}  // namespace

// Implements the mojom interface CacheStorageCache. It's owned by
// CacheStorageDispatcherHost and it's destroyed when client drops the mojo ptr
// which in turn removes from StrongBindingSet in CacheStorageDispatcherHost.
class CacheStorageDispatcherHost::CacheImpl
    : public blink::mojom::CacheStorageCache {
 public:
  explicit CacheImpl(CacheStorageCacheHandle cache_handle)
      : cache_handle_(std::move(cache_handle)), weak_factory_(this) {}

  ~CacheImpl() override = default;

  // blink::mojom::CacheStorageCache implementation:
  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::QueryParamsPtr match_params,
             MatchCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    cache->Match(
        std::move(request), std::move(match_params),
        base::BindOnce(&CacheImpl::OnCacheMatchCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnCacheMatchCallback(
      blink::mojom::CacheStorageCache::MatchCallback callback,
      blink::mojom::CacheStorageError error,
      blink::mojom::FetchAPIResponsePtr response) {
    if (error != CacheStorageError::kSuccess) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(error));
      return;
    }

    std::move(callback).Run(
        blink::mojom::MatchResult::NewResponse(std::move(response)));
  }

  void MatchAll(blink::mojom::FetchAPIRequestPtr request,
                blink::mojom::QueryParamsPtr match_params,
                MatchAllCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::MatchAllResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    cache->MatchAll(
        std::move(request), std::move(match_params),
        base::BindOnce(&CacheImpl::OnCacheMatchAllCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnCacheMatchAllCallback(
      blink::mojom::CacheStorageCache::MatchAllCallback callback,
      blink::mojom::CacheStorageError error,
      std::vector<blink::mojom::FetchAPIResponsePtr> responses) {
    if (error != CacheStorageError::kSuccess &&
        error != CacheStorageError::kErrorNotFound) {
      std::move(callback).Run(blink::mojom::MatchAllResult::NewStatus(error));
      return;
    }

    std::move(callback).Run(
        blink::mojom::MatchAllResult::NewResponses(std::move(responses)));
  }

  void Keys(blink::mojom::FetchAPIRequestPtr request,
            blink::mojom::QueryParamsPtr match_params,
            KeysCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::CacheKeysResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    cache->Keys(
        std::move(request), std::move(match_params),
        base::BindOnce(&CacheImpl::OnCacheKeysCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnCacheKeysCallback(
      blink::mojom::CacheStorageCache::KeysCallback callback,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<content::CacheStorageCache::Requests> requests) {
    if (error != CacheStorageError::kSuccess) {
      std::move(callback).Run(blink::mojom::CacheKeysResult::NewStatus(error));
      return;
    }
    std::vector<blink::mojom::FetchAPIRequestPtr> requests_;
    for (const auto& request : *requests) {
      requests_.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
    }

    std::move(callback).Run(
        blink::mojom::CacheKeysResult::NewKeys(std::move(requests_)));
  }

  void Batch(std::vector<blink::mojom::BatchOperationPtr> batch_operations,
             bool fail_on_duplicates,
             BatchCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(CacheStorageVerboseError::New(
          CacheStorageError::kErrorNotFound, base::nullopt));
      return;
    }
    cache->BatchOperation(
        std::move(batch_operations), fail_on_duplicates,
        base::BindOnce(&CacheImpl::OnCacheBatchCallback,
                       weak_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&CacheImpl::OnBadMessage, weak_factory_.GetWeakPtr(),
                       mojo::GetBadMessageCallback()));
  }

  void SetSideData(const GURL& url,
                   base::Time response_time,
                   const std::vector<uint8_t>& side_data,
                   SetSideDataCallback callback) override {
    content::CacheStorageCache* cache = cache_handle_.value();
    if (!cache) {
      std::move(callback).Run(blink::mojom::CacheStorageError::kErrorNotFound);
      return;
    }
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(side_data.size());
    if (!side_data.empty())
      memcpy(buffer->data(), &side_data.front(), side_data.size());
    cache->WriteSideData(std::move(callback), url, response_time,
                         std::move(buffer), side_data.size());
  }

  void OnCacheBatchCallback(
      blink::mojom::CacheStorageCache::BatchCallback callback,
      blink::mojom::CacheStorageVerboseErrorPtr error) {
    std::move(callback).Run(std::move(error));
  }

  void OnBadMessage(mojo::ReportBadMessageCallback bad_message_callback) {
    std::move(bad_message_callback).Run("CSDH_UNEXPECTED_OPERATION");
  }

  CacheStorageCacheHandle cache_handle_;
  base::WeakPtrFactory<CacheImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(CacheImpl);
};

// Implements the mojom interface CacheStorage. It's owned by the
// CacheStorageDispatcherHost.  The CacheStorageImpl is destroyed when the
// client drops its mojo ptr which in turn removes from StrongBindingSet in
// CacheStorageDispatcherHost.
class CacheStorageDispatcherHost::CacheStorageImpl final
    : public blink::mojom::CacheStorage {
 public:
  CacheStorageImpl(CacheStorageDispatcherHost* owner, const url::Origin& origin)
      : owner_(owner), origin_(origin), weak_factory_(this) {
    // The CacheStorageHandle is empty to start and lazy initialized on first
    // use via GetOrCreateCacheStorage().  In the future we could eagerly create
    // the backend when the mojo connection is created.
  }

  ~CacheStorageImpl() override = default;

  // Mojo CacheStorage Interface implementation:
  void Keys(blink::mojom::CacheStorage::KeysCallback callback) override {
    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    if (!cache_storage) {
      std::move(callback).Run(std::vector<base::string16>());
      return;
    }
    cache_storage->EnumerateCaches(base::BindOnce(
        [](blink::mojom::CacheStorage::KeysCallback callback,
           const CacheStorageIndex& cache_index) {
          std::vector<base::string16> string16s;
          for (const auto& metadata : cache_index.ordered_cache_metadata()) {
            string16s.push_back(base::UTF8ToUTF16(metadata.name));
          }
          std::move(callback).Run(string16s);
        },
        std::move(callback)));
  }

  void Delete(const base::string16& cache_name,
              blink::mojom::CacheStorage::DeleteCallback callback) override {
    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    if (!cache_storage) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }
    cache_storage->DoomCache(base::UTF16ToUTF8(cache_name),
                             std::move(callback));
  }

  void Has(const base::string16& cache_name,
           blink::mojom::CacheStorage::HasCallback callback) override {
    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    if (!cache_storage) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }
    cache_storage->HasCache(
        base::UTF16ToUTF8(cache_name),
        base::BindOnce(
            [](blink::mojom::CacheStorage::HasCallback callback, bool has_cache,
               CacheStorageError error) {
              if (!has_cache)
                error = CacheStorageError::kErrorNotFound;
              std::move(callback).Run(error);
            },
            std::move(callback)));
  }

  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::QueryParamsPtr match_params,
             blink::mojom::CacheStorage::MatchCallback callback) override {
    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    if (!cache_storage) {
      std::move(callback).Run(blink::mojom::MatchResult::NewStatus(
          CacheStorageError::kErrorNotFound));
      return;
    }

    auto on_match = BindOnce(
        [](blink::mojom::CacheStorage::MatchCallback callback,
           CacheStorageError error,
           blink::mojom::FetchAPIResponsePtr response) {
          if (error != CacheStorageError::kSuccess) {
            std::move(callback).Run(
                blink::mojom::MatchResult::NewStatus(error));
            return;
          }

          std::move(callback).Run(
              blink::mojom::MatchResult::NewResponse(std::move(response)));
        },
        std::move(callback));

    if (!match_params->cache_name) {
      cache_storage->MatchAllCaches(std::move(request), std::move(match_params),
                                    std::move(on_match));
      return;
    }
    std::string cache_name = base::UTF16ToUTF8(*match_params->cache_name);
    cache_storage->MatchCache(std::move(cache_name), std::move(request),
                              std::move(match_params), std::move(on_match));
  }

  void Open(const base::string16& cache_name,
            blink::mojom::CacheStorage::OpenCallback callback) override {
    content::CacheStorage* cache_storage = GetOrCreateCacheStorage();
    if (!cache_storage) {
      std::move(callback).Run(blink::mojom::OpenResult::NewStatus(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull)));
      return;
    }
    cache_storage->OpenCache(
        base::UTF16ToUTF8(cache_name),
        base::BindOnce(
            [](base::WeakPtr<CacheStorageImpl> self,
               blink::mojom::CacheStorage::OpenCallback callback,
               CacheStorageCacheHandle cache_handle, CacheStorageError error) {
              if (!self)
                return;

              if (error != CacheStorageError::kSuccess) {
                std::move(callback).Run(
                    blink::mojom::OpenResult::NewStatus(error));
                return;
              }

              blink::mojom::CacheStorageCacheAssociatedPtrInfo ptr_info;
              auto request = mojo::MakeRequest(&ptr_info);
              auto cache_impl =
                  std::make_unique<CacheImpl>(std::move(cache_handle));
              self->owner_->AddCacheBinding(std::move(cache_impl),
                                            std::move(request));

              std::move(callback).Run(
                  blink::mojom::OpenResult::NewCache(std::move(ptr_info)));
            },
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  // Helper method that returns the current CacheStorageHandle value.  If the
  // handle is closed, then it attempts to open a new CacheStorageHandle
  // automatically.  This automatic open is necessary to re-attach to the
  // backend after the browser storage has been wiped.
  content::CacheStorage* GetOrCreateCacheStorage() {
    DCHECK(owner_);
    if (!cache_storage_handle_.value())
      cache_storage_handle_ = owner_->OpenCacheStorage(origin_);
    return cache_storage_handle_.value();
  }

  // Owns this.
  CacheStorageDispatcherHost* const owner_;

  const url::Origin origin_;
  CacheStorageHandle cache_storage_handle_;

  base::WeakPtrFactory<CacheStorageImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(CacheStorageImpl);
};

CacheStorageDispatcherHost::CacheStorageDispatcherHost() = default;

CacheStorageDispatcherHost::~CacheStorageDispatcherHost() = default;

void CacheStorageDispatcherHost::Init(CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CacheStorageDispatcherHost::CreateCacheListener,
                     base::RetainedRef(this), base::RetainedRef(context)));
}

void CacheStorageDispatcherHost::CreateCacheListener(
    CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_ = context;
}

void CacheStorageDispatcherHost::AddBinding(
    blink::mojom::CacheStorageRequest request,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto impl = std::make_unique<CacheStorageImpl>(this, origin);
  bindings_.AddBinding(std::move(impl), std::move(request));
}

void CacheStorageDispatcherHost::AddCacheBinding(
    std::unique_ptr<CacheImpl> cache_impl,
    blink::mojom::CacheStorageCacheAssociatedRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  cache_bindings_.AddBinding(std::move(cache_impl), std::move(request));
}

CacheStorageHandle CacheStorageDispatcherHost::OpenCacheStorage(
    const url::Origin& origin) {
  if (!context_ || !context_->cache_manager() ||
      !OriginCanAccessCacheStorage(origin))
    return CacheStorageHandle();

  return context_->cache_manager()->OpenCacheStorage(
      origin, CacheStorageOwner::kCacheAPI);
}

}  // namespace content

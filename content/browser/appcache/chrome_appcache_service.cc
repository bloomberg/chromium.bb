// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/chrome_appcache_service.h"

#include <utility>

#include "base/files/file_path.h"
#include "content/browser/appcache/appcache_storage_impl.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "net/base/net_errors.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace content {

ChromeAppCacheService::ChromeAppCacheService(
    storage::QuotaManagerProxy* quota_manager_proxy,
    base::WeakPtr<StoragePartitionImpl> partition)
    : AppCacheServiceImpl(quota_manager_proxy, std::move(partition)) {}

void ChromeAppCacheService::Initialize(
    const base::FilePath& cache_path,
    BrowserContext* browser_context,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  cache_path_ = cache_path;
  DCHECK(browser_context);
  browser_context_ = browser_context;

  // Init our base class.
  AppCacheServiceImpl::Initialize(cache_path_);
  set_appcache_policy(this);
  set_special_storage_policy(special_storage_policy.get());
}

void ChromeAppCacheService::CreateBackend(
    int process_id,
    mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver) {
  // The process_id is the id of the RenderProcessHost, which can be reused for
  // a new renderer process if the previous renderer process was shutdown.
  // It can take some time after shutdown for the pipe error to propagate
  // and unregister the previous backend. Since the AppCacheService assumes
  // that there is one backend per process_id, we need to ensure that the
  // previous backend is unregistered by eagerly unbinding the pipe.
  Unbind(process_id);

  Bind(std::make_unique<AppCacheBackendImpl>(this, process_id),
       std::move(receiver), process_id);
}

void ChromeAppCacheService::CreateBackendForRequest(
    int process_id,
    blink::mojom::AppCacheBackendRequest request) {
  // Implicit conversion to mojo::PendingReceiver<T>.
  CreateBackend(process_id, std::move(request));
}

void ChromeAppCacheService::Bind(
    std::unique_ptr<blink::mojom::AppCacheBackend> backend,
    mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver,
    int process_id) {
  DCHECK(process_receivers_.find(process_id) == process_receivers_.end());
  process_receivers_[process_id] =
      receivers_.Add(std::move(backend), std::move(receiver));
}

void ChromeAppCacheService::Unbind(int process_id) {
  auto it = process_receivers_.find(process_id);
  if (it != process_receivers_.end()) {
    receivers_.Remove(it->second);
    DCHECK(process_receivers_.find(process_id) == process_receivers_.end());
  }
}

void ChromeAppCacheService::UnregisterBackend(
    AppCacheBackendImpl* backend_impl) {
  int process_id = backend_impl->process_id();
  process_receivers_.erase(process_receivers_.find(process_id));
  AppCacheServiceImpl::UnregisterBackend(backend_impl);
}

void ChromeAppCacheService::Shutdown() {
  receivers_.Clear();
  partition_ = nullptr;
}

bool ChromeAppCacheService::CanLoadAppCache(const GURL& manifest_url,
                                            const GURL& first_party) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetContentClient()->browser()->AllowAppCache(manifest_url, first_party,
                                                      browser_context_);
}

bool ChromeAppCacheService::CanCreateAppCache(
    const GURL& manifest_url, const GURL& first_party) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetContentClient()->browser()->AllowAppCache(manifest_url, first_party,
                                                      browser_context_);
}

ChromeAppCacheService::~ChromeAppCacheService() {}

void ChromeAppCacheService::DeleteOnCorrectThread() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
    return;
  }
  if (BrowserThread::IsThreadInitialized(BrowserThread::UI)) {
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
    return;
  }
  // Better to leak than crash on shutdown.
}

}  // namespace content

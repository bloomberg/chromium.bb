// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_backend_impl.h"

#include <memory>
#include <vector>

#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace content {

AppCacheBackendImpl::AppCacheBackendImpl(AppCacheServiceImpl* service,
                                         int process_id)
    : service_(service),
      frontend_proxy_(process_id),
      frontend_(&frontend_proxy_),
      process_id_(process_id) {
  DCHECK(service);
  service_->RegisterBackend(this);
}

AppCacheBackendImpl::~AppCacheBackendImpl() {
  hosts_.clear();
  service_->UnregisterBackend(this);
}

void AppCacheBackendImpl::RegisterHost(int32_t id) {
  // The AppCacheHost could have been precreated in which case we want to
  // register it with the backend here.
  std::unique_ptr<AppCacheHost> host =
      AppCacheNavigationHandleCore::GetPrecreatedHost(id);
  if (host) {
    RegisterPrecreatedHost(std::move(host));
    return;
  }

  if (GetHost(id)) {
    mojo::ReportBadMessage("ACDH_REGISTER");
    return;
  }

  hosts_[id] =
      std::make_unique<AppCacheHost>(id, process_id(), frontend_, service_);
}

void AppCacheBackendImpl::UnregisterHost(int32_t id) {
  if (!hosts_.erase(id))
    mojo::ReportBadMessage("ACDH_UNREGISTER");
}

void AppCacheBackendImpl::SetSpawningHostId(int32_t host_id,
                                            int32_t spawning_host_id) {
  AppCacheHost* host = GetHost(host_id);
  if (!host) {
    mojo::ReportBadMessage("ACDH_SET_SPAWNING");
    return;
  }
  host->SetSpawningHostId(process_id_, spawning_host_id);
}

void AppCacheBackendImpl::SelectCache(
    int32_t host_id,
    const GURL& document_url,
    const int64_t cache_document_was_loaded_from,
    const GURL& manifest_url) {
  AppCacheHost* host = GetHost(host_id);
  if (!host) {
    mojo::ReportBadMessage("ACDH_SELECT_CACHE");
    return;
  }
  host->SelectCache(document_url, cache_document_was_loaded_from, manifest_url);
}

void AppCacheBackendImpl::SelectCacheForSharedWorker(int32_t host_id,
                                                     int64_t appcache_id) {
  AppCacheHost* host = GetHost(host_id);
  if (!host) {
    mojo::ReportBadMessage("ACDH_SELECT_CACHE_FOR_SHARED_WORKER");
    return;
  }
  host->SelectCacheForSharedWorker(appcache_id);
}

void AppCacheBackendImpl::MarkAsForeignEntry(
    int32_t host_id,
    const GURL& document_url,
    int64_t cache_document_was_loaded_from) {
  AppCacheHost* host = GetHost(host_id);
  if (!host) {
    mojo::ReportBadMessage("ACDH_MARK_AS_FOREIGN_ENTRY");
    return;
  }
  host->MarkAsForeignEntry(document_url, cache_document_was_loaded_from);
}

void AppCacheBackendImpl::GetStatus(int32_t host_id,
                                    GetStatusCallback callback) {
  AppCacheHost* host = GetHost(host_id);
  if (!host) {
    mojo::ReportBadMessage("ACDH_GET_STATUS");
    std::move(callback).Run(
        blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED);
    return;
  }

  host->GetStatusWithCallback(std::move(callback));
}

void AppCacheBackendImpl::StartUpdate(int32_t host_id,
                                      StartUpdateCallback callback) {
  AppCacheHost* host = GetHost(host_id);
  if (!host) {
    mojo::ReportBadMessage("ACDH_START_UPDATE");
    std::move(callback).Run(false);
    return;
  }

  host->StartUpdateWithCallback(std::move(callback));
}

void AppCacheBackendImpl::SwapCache(int32_t host_id,
                                    SwapCacheCallback callback) {
  AppCacheHost* host = GetHost(host_id);
  if (!host) {
    mojo::ReportBadMessage("ACDH_SWAP_CACHE");
    std::move(callback).Run(false);
    return;
  }

  host->SwapCacheWithCallback(std::move(callback));
}

void AppCacheBackendImpl::GetResourceList(int32_t host_id,
                                          GetResourceListCallback callback) {
  std::vector<blink::mojom::AppCacheResourceInfo> params;
  std::vector<blink::mojom::AppCacheResourceInfoPtr> out;

  AppCacheHost* host = GetHost(host_id);
  if (host)
    host->GetResourceList(&params);

  // Box up params for output.
  out.reserve(params.size());
  for (auto& p : params) {
    out.emplace_back(base::in_place, std::move(p));
  }
  std::move(callback).Run(std::move(out));
}

void AppCacheBackendImpl::RegisterPrecreatedHost(
    std::unique_ptr<AppCacheHost> host) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(host.get());
  DCHECK(hosts_.find(host->host_id()) == hosts_.end());
  // Switch the frontend proxy so that the host can make IPC calls from
  // here on.
  host->set_frontend(frontend_);
  hosts_[host->host_id()] = std::move(host);
}

void AppCacheBackendImpl::RegisterHostForTesting(int32_t id) {
  hosts_[id] =
      std::make_unique<AppCacheHost>(id, process_id(), frontend_, service_);
}

}  // namespace content

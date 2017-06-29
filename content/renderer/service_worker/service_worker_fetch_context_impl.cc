// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_fetch_context_impl.h"

#include "content/child/request_extra_data.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/web_url_loader_impl.h"

namespace content {

ServiceWorkerFetchContextImpl::ServiceWorkerFetchContextImpl(
    const GURL& worker_script_url,
    mojom::WorkerURLLoaderFactoryProviderPtrInfo provider_info,
    int service_worker_provider_id)
    : worker_script_url_(worker_script_url),
      provider_info_(std::move(provider_info)),
      service_worker_provider_id_(service_worker_provider_id) {}

ServiceWorkerFetchContextImpl::~ServiceWorkerFetchContextImpl() {}

void ServiceWorkerFetchContextImpl::InitializeOnWorkerThread(
    base::SingleThreadTaskRunner* loading_task_runner) {
  DCHECK(provider_info_.is_valid());
  resource_dispatcher_ =
      base::MakeUnique<ResourceDispatcher>(nullptr, loading_task_runner);
  provider_.Bind(std::move(provider_info_));
  provider_->GetURLLoaderFactory(mojo::MakeRequest(&url_loader_factory_));
}

std::unique_ptr<blink::WebURLLoader>
ServiceWorkerFetchContextImpl::CreateURLLoader(
    const blink::WebURLRequest& request,
    base::SingleThreadTaskRunner* task_runner) {
  return base::MakeUnique<content::WebURLLoaderImpl>(
      resource_dispatcher_.get(), task_runner, url_loader_factory_.get());
}

void ServiceWorkerFetchContextImpl::WillSendRequest(
    blink::WebURLRequest& request) {
  RequestExtraData* extra_data = new RequestExtraData();
  extra_data->set_service_worker_provider_id(service_worker_provider_id_);
  extra_data->set_originated_from_service_worker(true);
  extra_data->set_initiated_in_secure_context(true);
  request.SetExtraData(extra_data);
}

bool ServiceWorkerFetchContextImpl::IsControlledByServiceWorker() const {
  return false;
}

void ServiceWorkerFetchContextImpl::SetDataSaverEnabled(bool enabled) {
  is_data_saver_enabled_ = enabled;
}

bool ServiceWorkerFetchContextImpl::IsDataSaverEnabled() const {
  return is_data_saver_enabled_;
}

blink::WebURL ServiceWorkerFetchContextImpl::FirstPartyForCookies() const {
  // According to the spec, we can use the |worker_script_url_| for
  // FirstPartyForCookies, because "site for cookies" for the service worker is
  // the service worker's origin's host's registrable domain.
  // https://tools.ietf.org/html/draft-west-first-party-cookies-07#section-2.1.2
  return worker_script_url_;
}

}  // namespace content

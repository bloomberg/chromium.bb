// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_subresource_url_factory.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_url_loader_job.h"
#include "content/browser/appcache/appcache_url_loader_request.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

// Implements the URLLoaderFactory mojom for AppCache requests.
AppCacheSubresourceURLFactory::AppCacheSubresourceURLFactory(
    URLLoaderFactoryGetter* default_url_loader_factory_getter,
    base::WeakPtr<AppCacheHost> host)
    : default_url_loader_factory_getter_(default_url_loader_factory_getter),
      appcache_host_(host),
      weak_factory_(this) {
  bindings_.set_connection_error_handler(
      base::Bind(&AppCacheSubresourceURLFactory::OnConnectionError,
                 base::Unretained(this)));
}

AppCacheSubresourceURLFactory::~AppCacheSubresourceURLFactory() {}

// static
void AppCacheSubresourceURLFactory::CreateURLLoaderFactory(
    URLLoaderFactoryGetter* default_url_loader_factory_getter,
    base::WeakPtr<AppCacheHost> host,
    mojom::URLLoaderFactoryPtr* loader_factory) {
  DCHECK(host.get());
  mojom::URLLoaderFactoryRequest request = mojo::MakeRequest(loader_factory);

  // This instance is effectively reference counted by the number of pipes open
  // to it and will get deleted when all clients drop their connections.
  // Please see OnConnectionError() for details.
  auto* impl = new AppCacheSubresourceURLFactory(
      default_url_loader_factory_getter, host);
  impl->Clone(std::move(request));

  // Save the factory in the host to ensure that we don't create it again when
  // the cache is selected, etc.
  host->SetAppCacheSubresourceFactory(impl);
}

void AppCacheSubresourceURLFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest url_loader_request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DLOG(WARNING) << "Received request for loading : " << request.url.spec();

  // If the host is invalid, it means that the renderer has probably died.
  // (Frame has navigated elsewhere?)
  if (!appcache_host_.get()) {
    NotifyError(std::move(client), net::ERR_FAILED);
    return;
  }

  std::unique_ptr<AppCacheRequestHandler> handler =
      appcache_host_->CreateRequestHandler(
          AppCacheURLLoaderRequest::Create(request), request.resource_type,
          request.should_reset_appcache);
  if (!handler) {
    NotifyError(std::move(client), net::ERR_FAILED);
    return;
  }

  std::unique_ptr<SubresourceLoadInfo> load_info(new SubresourceLoadInfo());
  load_info->url_loader_request = std::move(url_loader_request);
  load_info->routing_id = routing_id;
  load_info->request_id = request_id;
  load_info->options = options;
  load_info->request = request;
  load_info->client = std::move(client);
  load_info->traffic_annotation = traffic_annotation;

  if (handler->MaybeCreateSubresourceLoader(
          std::move(load_info), default_url_loader_factory_getter_.get(),
          this)) {
    // The handler is owned by the job and will be destroyed with the job.
    handler.release();
  }
}

void AppCacheSubresourceURLFactory::Clone(
    mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

base::WeakPtr<AppCacheSubresourceURLFactory>
AppCacheSubresourceURLFactory::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppCacheSubresourceURLFactory::Restart(
    const net::RedirectInfo& redirect_info,
    std::unique_ptr<AppCacheRequestHandler> subresource_handler,
    std::unique_ptr<SubresourceLoadInfo> subresource_load_info) {
  if (!appcache_host_.get())
    return;

  // Should not hit the redirect limit.
  DCHECK(subresource_load_info->redirect_limit > 0);

  subresource_load_info->request.url = redirect_info.new_url;
  subresource_load_info->request.method = redirect_info.new_method;
  subresource_load_info->request.referrer = GURL(redirect_info.new_referrer);
  subresource_load_info->request.site_for_cookies =
      redirect_info.new_site_for_cookies;

  if (subresource_handler->MaybeCreateSubresourceLoader(
          std::move(subresource_load_info),
          default_url_loader_factory_getter_.get(), this)) {
    // The handler is owned by the job and will be destroyed with the job.
    subresource_handler.release();
  }
}

void AppCacheSubresourceURLFactory::OnConnectionError() {
  if (bindings_.empty())
    delete this;
}

void AppCacheSubresourceURLFactory::NotifyError(
    mojom::URLLoaderClientPtr client,
    int error_code) {
  ResourceRequestCompletionStatus request_result;
  request_result.error_code = error_code;
  client->OnComplete(request_result);
}

}  // namespace content

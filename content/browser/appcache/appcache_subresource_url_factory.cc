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
      appcache_host_(host) {
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
  mojom::URLLoaderFactoryRequest request = mojo::MakeRequest(loader_factory);

  // This instance is effectively reference counted by the number of pipes open
  // to it and will get deleted when all clients drop their connections.
  // Please see OnConnectionError() for details.
  auto* impl = new AppCacheSubresourceURLFactory(
      default_url_loader_factory_getter, host);
  impl->Clone(std::move(request));
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

  // TODO(ananta/michaeln)
  // We need to handle redirects correctly, i.e every subresource redirect
  // could potentially be served out of the cache.
  if (handler->MaybeCreateSubresourceLoader(
          std::move(load_info), default_url_loader_factory_getter_.get())) {
    // The handler is owned by the job and will be destoryed when the job is
    // destroyed.
    handler.release();
  }
}

void AppCacheSubresourceURLFactory::Clone(
    mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
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

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
    mojom::URLLoaderFactoryRequest request,
    URLLoaderFactoryGetter* default_url_loader_factory_getter,
    base::WeakPtr<AppCacheHost> host)
    : binding_(this, std::move(request)),
      default_url_loader_factory_getter_(default_url_loader_factory_getter),
      appcache_host_(host) {
  binding_.set_connection_error_handler(
      base::Bind(&AppCacheSubresourceURLFactory::OnConnectionError,
                 base::Unretained(this)));
}

AppCacheSubresourceURLFactory::~AppCacheSubresourceURLFactory() {}

// static
AppCacheSubresourceURLFactory*
AppCacheSubresourceURLFactory::CreateURLLoaderFactory(
    URLLoaderFactoryGetter* default_url_loader_factory_getter,
    base::WeakPtr<AppCacheHost> host,
    mojom::URLLoaderFactoryPtr* loader_factory) {
  mojom::URLLoaderFactoryRequest request = mojo::MakeRequest(loader_factory);

  // This instance will get deleted when the client drops the connection.
  // Please see OnConnectionError() for details.
  return new AppCacheSubresourceURLFactory(
      std::move(request), default_url_loader_factory_getter, host);
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
  if (!appcache_host_.get())
    return;

  std::unique_ptr<AppCacheRequestHandler> handler =
      appcache_host_->CreateRequestHandler(
          AppCacheURLLoaderRequest::Create(request), request.resource_type,
          request.should_reset_appcache);
  if (!handler) {
    ResourceRequestCompletionStatus request_result;
    request_result.error_code = net::ERR_FAILED;
    client->OnComplete(request_result);
    return;
  }

  handler->set_network_url_loader_factory_getter(
      default_url_loader_factory_getter_.get());

  std::unique_ptr<SubresourceLoadInfo> load_info(new SubresourceLoadInfo());
  load_info->url_loader_request = std::move(url_loader_request);
  load_info->routing_id = routing_id;
  load_info->request_id = request_id;
  load_info->options = options;
  load_info->request = request;
  load_info->client = std::move(client);
  load_info->traffic_annotation = traffic_annotation;

  handler->SetSubresourceRequestLoadInfo(std::move(load_info));

  AppCacheJob* job = handler->MaybeLoadResource(nullptr);
  if (job) {
    // The handler is owned by the job.
    job->AsURLLoaderJob()->set_request_handler(std::move(handler));
  }
}

void AppCacheSubresourceURLFactory::SyncLoad(int32_t routing_id,
                                             int32_t request_id,
                                             const ResourceRequest& request,
                                             SyncLoadCallback callback) {
  NOTREACHED();
}

void AppCacheSubresourceURLFactory::OnConnectionError() {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

}  // namespace content

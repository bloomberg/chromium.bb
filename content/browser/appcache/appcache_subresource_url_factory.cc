// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_subresource_url_factory.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_url_loader_job.h"
#include "content/browser/appcache/appcache_url_loader_request.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/resource_request.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

// Implements the URLLoaderFactory mojom for AppCache requests.
AppCacheSubresourceURLFactory::AppCacheSubresourceURLFactory(
    mojom::URLLoaderFactoryRequest request,
    URLLoaderFactoryGetter* default_url_loader_factory_getter)
    : binding_(this, std::move(request)),
      default_url_loader_factory_getter_(default_url_loader_factory_getter) {
  binding_.set_connection_error_handler(
      base::Bind(&AppCacheSubresourceURLFactory::OnConnectionError,
                 base::Unretained(this)));
}

AppCacheSubresourceURLFactory::~AppCacheSubresourceURLFactory() {}

// static
mojom::URLLoaderFactoryPtr
AppCacheSubresourceURLFactory::CreateURLLoaderFactory(
    URLLoaderFactoryGetter* default_url_loader_factory_getter) {
  mojom::URLLoaderFactoryPtr loader_factory;
  mojom::URLLoaderFactoryRequest request = mojo::MakeRequest(&loader_factory);

  // This instance will get deleted when the client drops the connection.
  // Please see OnConnectionError() for details.
  new AppCacheSubresourceURLFactory(std::move(request),
                                    default_url_loader_factory_getter);
  return loader_factory;
}

void AppCacheSubresourceURLFactory::CreateLoaderAndStart(
    mojom::URLLoaderAssociatedRequest url_loader_request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DLOG(WARNING) << "Received request for loading : " << request.url.spec();
  default_url_loader_factory_getter_->GetNetworkFactory()
      ->get()
      ->CreateLoaderAndStart(mojom::URLLoaderAssociatedRequest(), routing_id,
                             request_id, options, request, std::move(client),
                             traffic_annotation);
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
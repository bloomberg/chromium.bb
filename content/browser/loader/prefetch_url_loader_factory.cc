// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader_factory.h"

#include "base/feature_list.h"
#include "content/browser/loader/prefetch_url_loader.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/wrapper_shared_url_loader_factory.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/public/cpp/features.h"

namespace content {

PrefetchURLLoaderFactory::PrefetchURLLoaderFactory(
    scoped_refptr<URLLoaderFactoryGetter> factory_getter)
    : loader_factory_getter_(std::move(factory_getter)) {}

void PrefetchURLLoaderFactory::ConnectToService(
    blink::mojom::PrefetchURLLoaderServiceRequest request) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // TODO(https://crbug.com/813479): Investigate why we need this branch
    // and remove it after we found the root cause. Bind calls to the
    // BindRegistry should come on to the IO thread, but it looks we hit
    // here in browser tests (but not in full chrome build), i.e.
    // in content/browser/loader/prefetch_browsertest.cc.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&PrefetchURLLoaderFactory::ConnectToService, this,
                       std::move(request)));
    return;
  }
  service_bindings_.AddBinding(this, std::move(request));
}

void PrefetchURLLoaderFactory::GetFactory(
    network::mojom::URLLoaderFactoryRequest request) {
  Clone(std::move(request));
}

void PrefetchURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    network::mojom::URLLoaderFactory* network_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(RESOURCE_TYPE_PREFETCH, resource_request.resource_type);

  if (prefetch_load_callback_for_testing_)
    prefetch_load_callback_for_testing_.Run();

  // For now we strongly bind the loader to the request, while we can
  // also possibly make the new loader owned by the factory so that
  // they can live longer than the client (i.e. run in detached mode).
  // TODO(kinuko): Revisit this.
  mojo::MakeStrongBinding(
      std::make_unique<PrefetchURLLoader>(
          routing_id, request_id, options, resource_request, std::move(client),
          traffic_annotation, network_loader_factory),
      std::move(request));
}

PrefetchURLLoaderFactory::~PrefetchURLLoaderFactory() = default;

void PrefetchURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
  if (!network_loader_factory_ || network_loader_factory_.encountered_error()) {
    loader_factory_getter_->GetNetworkFactory()->Clone(
        mojo::MakeRequest(&network_loader_factory_));
  }
  CreateLoaderAndStart(std::move(request), routing_id, request_id, options,
                       resource_request, std::move(client), traffic_annotation,
                       network_loader_factory_.get());
}

void PrefetchURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace content

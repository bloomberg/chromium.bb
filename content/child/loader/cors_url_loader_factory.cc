// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/loader/cors_url_loader_factory.h"

#include "content/child/loader/cors_url_loader.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

void CORSURLLoaderFactory::CreateAndBind(
    PossiblyAssociatedInterfacePtr<mojom::URLLoaderFactory>
        network_loader_factory,
    mojom::URLLoaderFactoryRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<CORSURLLoaderFactory>(std::move(network_loader_factory)),
      std::move(request));
}

CORSURLLoaderFactory::CORSURLLoaderFactory(
    PossiblyAssociatedInterfacePtr<mojom::URLLoaderFactory>
        network_loader_factory)
    : network_loader_factory_(std::move(network_loader_factory)) {}

CORSURLLoaderFactory::~CORSURLLoaderFactory() {}

void CORSURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  mojo::MakeStrongBinding(
      base::MakeUnique<CORSURLLoader>(
          routing_id, request_id, options, resource_request, std::move(client),
          traffic_annotation, network_loader_factory_.get()),
      std::move(request));
}

void CORSURLLoaderFactory::SyncLoad(int32_t routing_id,
                                    int32_t request_id,
                                    const ResourceRequest& resource_request,
                                    SyncLoadCallback callback) {
  network_loader_factory_->SyncLoad(routing_id, request_id, resource_request,
                                    std::move(callback));
}

}  // namespace content

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/cors_url_loader_factory.h"

#include "content/renderer/loader/cors_url_loader.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace content {

void CORSURLLoaderFactory::CreateAndBind(
    PossiblyAssociatedInterfacePtrInfo<network::mojom::URLLoaderFactory>
        network_loader_factory,
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK(network_loader_factory);

  // This object will be destroyed when all pipes bound to it are closed.
  // See OnConnectionError().
  auto* impl = new CORSURLLoaderFactory(std::move(network_loader_factory));
  impl->Clone(std::move(request));
}

CORSURLLoaderFactory::CORSURLLoaderFactory(
    PossiblyAssociatedInterfacePtrInfo<network::mojom::URLLoaderFactory>
        network_loader_factory) {
  // Binding |this| as an unretained pointer is safe because |bindings_| shares
  // this object's lifetime.
  bindings_.set_connection_error_handler(base::Bind(
      &CORSURLLoaderFactory::OnConnectionError, base::Unretained(this)));
  loader_bindings_.set_connection_error_handler(base::Bind(
      &CORSURLLoaderFactory::OnConnectionError, base::Unretained(this)));

  network_loader_factory_.Bind(std::move(network_loader_factory));
}

CORSURLLoaderFactory::~CORSURLLoaderFactory() {}

void CORSURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // Instances of CORSURLLoader are owned by this class and their pipe so that
  // they can share |network_loader_factory_|.
  loader_bindings_.AddBinding(
      std::make_unique<CORSURLLoader>(
          routing_id, request_id, options, resource_request, std::move(client),
          traffic_annotation, network_loader_factory_.get()),
      std::move(request));
}

void CORSURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void CORSURLLoaderFactory::OnConnectionError() {
  if (bindings_.empty() && loader_bindings_.empty())
    delete this;
}

}  // namespace content

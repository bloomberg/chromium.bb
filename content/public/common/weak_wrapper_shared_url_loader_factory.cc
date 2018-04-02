// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/weak_wrapper_shared_url_loader_factory.h"

#include "content/common/wrapper_shared_url_loader_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

WeakWrapperSharedURLLoaderFactory::WeakWrapperSharedURLLoaderFactory(
    network::mojom::URLLoaderFactory* factory_ptr)
    : factory_ptr_(factory_ptr) {}

void WeakWrapperSharedURLLoaderFactory::Detach() {
  factory_ptr_ = nullptr;
}

void WeakWrapperSharedURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!factory_ptr_)
    return;
  factory_ptr_->CreateLoaderAndStart(std::move(loader), routing_id, request_id,
                                     options, request, std::move(client),
                                     traffic_annotation);
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
WeakWrapperSharedURLLoaderFactory::Clone() {
  network::mojom::URLLoaderFactoryPtrInfo factory_ptr_info;
  if (factory_ptr_)
    factory_ptr_->Clone(mojo::MakeRequest(&factory_ptr_info));
  return std::make_unique<WrapperSharedURLLoaderFactoryInfo>(
      std::move(factory_ptr_info));
}

WeakWrapperSharedURLLoaderFactory::~WeakWrapperSharedURLLoaderFactory() =
    default;

}  // namespace content

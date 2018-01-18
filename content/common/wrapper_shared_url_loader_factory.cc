// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/wrapper_shared_url_loader_factory.h"

#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

WrapperSharedURLLoaderFactoryInfo::WrapperSharedURLLoaderFactoryInfo() =
    default;

WrapperSharedURLLoaderFactoryInfo::WrapperSharedURLLoaderFactoryInfo(
    network::mojom::URLLoaderFactoryPtrInfo factory_ptr_info)
    : factory_ptr_info_(std::move(factory_ptr_info)) {}

WrapperSharedURLLoaderFactoryInfo::~WrapperSharedURLLoaderFactoryInfo() =
    default;

scoped_refptr<SharedURLLoaderFactory>
WrapperSharedURLLoaderFactoryInfo::CreateFactory() {
  return base::MakeRefCounted<WrapperSharedURLLoaderFactory>(
      std::move(factory_ptr_info_));
}

WrapperSharedURLLoaderFactory::WrapperSharedURLLoaderFactory(
    network::mojom::URLLoaderFactoryPtr factory_ptr)
    : factory_ptr_(std::move(factory_ptr)) {}

WrapperSharedURLLoaderFactory::WrapperSharedURLLoaderFactory(
    network::mojom::URLLoaderFactoryPtrInfo factory_ptr_info)
    : factory_ptr_(std::move(factory_ptr_info)) {}

void WrapperSharedURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    const Constraints& constraints) {
  if (!factory_ptr_)
    return;
  factory_ptr_->CreateLoaderAndStart(std::move(loader), routing_id, request_id,
                                     options, request, std::move(client),
                                     traffic_annotation);
}

std::unique_ptr<SharedURLLoaderFactoryInfo>
WrapperSharedURLLoaderFactory::Clone() {
  network::mojom::URLLoaderFactoryPtrInfo factory_ptr_info;
  if (factory_ptr_)
    factory_ptr_->Clone(mojo::MakeRequest(&factory_ptr_info));
  return std::make_unique<WrapperSharedURLLoaderFactoryInfo>(
      std::move(factory_ptr_info));
}

WrapperSharedURLLoaderFactory::~WrapperSharedURLLoaderFactory() = default;

}  // namespace content

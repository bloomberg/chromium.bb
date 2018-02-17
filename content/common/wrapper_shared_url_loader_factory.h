// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WRAPPER_SHARED_URL_LOADER_FACTORY_H_
#define CONTENT_COMMON_WRAPPER_SHARED_URL_LOADER_FACTORY_H_

#include "content/common/content_export.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

// A SharedURLLoaderFactoryInfo implementation that wraps a
// network::mojom::URLLoaderFactoryPtrInfo.
class CONTENT_EXPORT WrapperSharedURLLoaderFactoryInfo
    : public SharedURLLoaderFactoryInfo {
 public:
  WrapperSharedURLLoaderFactoryInfo();
  explicit WrapperSharedURLLoaderFactoryInfo(
      network::mojom::URLLoaderFactoryPtrInfo factory_ptr_info);

  ~WrapperSharedURLLoaderFactoryInfo() override;

 private:
  // SharedURLLoaderFactoryInfo implementation.
  scoped_refptr<SharedURLLoaderFactory> CreateFactory() override;

  network::mojom::URLLoaderFactoryPtrInfo factory_ptr_info_;
};

// A SharedURLLoaderFactory implementation that wraps a
// PtrTemplateType<network::mojom::URLLoaderFactory>.
template <template <typename> class PtrTemplateType>
class WrapperSharedURLLoaderFactoryBase : public SharedURLLoaderFactory {
 public:
  using PtrType = PtrTemplateType<network::mojom::URLLoaderFactory>;
  using PtrInfoType = typename PtrType::PtrInfoType;

  explicit WrapperSharedURLLoaderFactoryBase(PtrType factory_ptr)
      : factory_ptr_(std::move(factory_ptr)) {}

  explicit WrapperSharedURLLoaderFactoryBase(PtrInfoType factory_ptr_info)
      : factory_ptr_(std::move(factory_ptr_info)) {}

  // SharedURLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    if (!factory_ptr_)
      return;
    factory_ptr_->CreateLoaderAndStart(std::move(loader), routing_id,
                                       request_id, options, request,
                                       std::move(client), traffic_annotation);
  }

  std::unique_ptr<SharedURLLoaderFactoryInfo> Clone() override {
    network::mojom::URLLoaderFactoryPtrInfo factory_ptr_info;
    if (factory_ptr_)
      factory_ptr_->Clone(mojo::MakeRequest(&factory_ptr_info));
    return std::make_unique<WrapperSharedURLLoaderFactoryInfo>(
        std::move(factory_ptr_info));
  }

 private:
  ~WrapperSharedURLLoaderFactoryBase() override = default;

  PtrType factory_ptr_;
};

using WrapperSharedURLLoaderFactory =
    WrapperSharedURLLoaderFactoryBase<mojo::InterfacePtr>;
using AssociatedWrapperSharedURLLoaderFactory =
    WrapperSharedURLLoaderFactoryBase<mojo::AssociatedInterfacePtr>;
using PossiblyAssociatedWrapperSharedURLLoaderFactory =
    WrapperSharedURLLoaderFactoryBase<PossiblyAssociatedInterfacePtr>;

}  // namespace content

#endif  // CONTENT_COMMON_WRAPPER_URL_LOADER_FACTORY_H_

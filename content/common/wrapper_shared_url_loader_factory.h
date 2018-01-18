// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WRAPPER_SHARED_URL_LOADER_FACTORY_H_
#define CONTENT_COMMON_WRAPPER_SHARED_URL_LOADER_FACTORY_H_

#include "content/common/content_export.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace content {

// A SharedURLLoaderFactoryInfo implementation that wraps a
// mojom::URLLoaderFactoryPtrInfo.
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
// mojom::URLLoaderFactoryPtr.
class CONTENT_EXPORT WrapperSharedURLLoaderFactory
    : public SharedURLLoaderFactory {
 public:
  explicit WrapperSharedURLLoaderFactory(
      network::mojom::URLLoaderFactoryPtr factory_ptr);
  explicit WrapperSharedURLLoaderFactory(
      network::mojom::URLLoaderFactoryPtrInfo factory_ptr_info);

  // SharedURLLoaderFactory implementation.
  void CreateLoaderAndStart(
      network::mojom::URLLoaderRequest loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const Constraints& constraints) override;

  std::unique_ptr<SharedURLLoaderFactoryInfo> Clone() override;

 private:
  ~WrapperSharedURLLoaderFactory() override;

  network::mojom::URLLoaderFactoryPtr factory_ptr_;
};

}  // namespace content

#endif  // CONTENT_COMMON_WRAPPER_URL_LOADER_FACTORY_H_

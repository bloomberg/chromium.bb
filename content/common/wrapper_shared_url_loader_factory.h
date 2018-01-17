// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WRAPPER_SHARED_URL_LOADER_FACTORY_H_
#define CONTENT_COMMON_WRAPPER_SHARED_URL_LOADER_FACTORY_H_

#include "content/common/content_export.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "content/public/common/url_loader_factory.mojom.h"

namespace content {

// A SharedURLLoaderFactoryInfo implementation that wraps a
// mojom::URLLoaderFactoryPtrInfo.
class CONTENT_EXPORT WrapperSharedURLLoaderFactoryInfo
    : public SharedURLLoaderFactoryInfo {
 public:
  WrapperSharedURLLoaderFactoryInfo();
  explicit WrapperSharedURLLoaderFactoryInfo(
      mojom::URLLoaderFactoryPtrInfo factory_ptr_info);

  ~WrapperSharedURLLoaderFactoryInfo() override;

 private:
  // SharedURLLoaderFactoryInfo implementation.
  scoped_refptr<SharedURLLoaderFactory> CreateFactory() override;

  mojom::URLLoaderFactoryPtrInfo factory_ptr_info_;
};

// A SharedURLLoaderFactory implementation that wraps a
// mojom::URLLoaderFactoryPtr.
class CONTENT_EXPORT WrapperSharedURLLoaderFactory
    : public SharedURLLoaderFactory {
 public:
  explicit WrapperSharedURLLoaderFactory(
      mojom::URLLoaderFactoryPtr factory_ptr);
  explicit WrapperSharedURLLoaderFactory(
      mojom::URLLoaderFactoryPtrInfo factory_ptr_info);

  // SharedURLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojom::URLLoaderRequest loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const Constraints& constraints) override;

  std::unique_ptr<SharedURLLoaderFactoryInfo> Clone() override;

 private:
  ~WrapperSharedURLLoaderFactory() override;

  mojom::URLLoaderFactoryPtr factory_ptr_;
};

}  // namespace content

#endif  // CONTENT_COMMON_WRAPPER_URL_LOADER_FACTORY_H_

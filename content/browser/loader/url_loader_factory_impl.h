// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/url_loader_factory.mojom.h"

namespace content {

class ResourceRequesterInfo;

// This class is an implementation of mojom::URLLoaderFactory that creates
// a mojom::URLLoader. This class is instantiated only for Service Worker
// navigation preload or test caseses.
class URLLoaderFactoryImpl final : public mojom::URLLoaderFactory {
 public:
  ~URLLoaderFactoryImpl() override;

  void CreateLoaderAndStart(
      mojom::URLLoaderAssociatedRequest request,
      int32_t routing_id,
      int32_t request_id,
      const ResourceRequest& url_request,
      mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info) override;
  void SyncLoad(int32_t routing_id,
                int32_t request_id,
                const ResourceRequest& request,
                const SyncLoadCallback& callback) override;

  static void CreateLoaderAndStart(
      ResourceRequesterInfo* requester_info,
      mojom::URLLoaderAssociatedRequest request,
      int32_t routing_id,
      int32_t request_id,
      const ResourceRequest& url_request,
      mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info);
  static void SyncLoad(ResourceRequesterInfo* requester_info,
                       int32_t routing_id,
                       int32_t request_id,
                       const ResourceRequest& request,
                       const SyncLoadCallback& callback);

  // Creates a URLLoaderFactoryImpl instance. The instance is held by the
  // StrongBinding in it, so this function doesn't return the instance.
  CONTENT_EXPORT static void Create(
      scoped_refptr<ResourceRequesterInfo> requester_info,
      mojom::URLLoaderFactoryRequest request);

 private:
  explicit URLLoaderFactoryImpl(
      scoped_refptr<ResourceRequesterInfo> requester_info);

  scoped_refptr<ResourceRequesterInfo> requester_info_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_

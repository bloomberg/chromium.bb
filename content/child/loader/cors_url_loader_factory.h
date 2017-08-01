// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_LOADER_CORS_URL_LOADER_FACTORY_H_
#define CONTENT_CHILD_LOADER_CORS_URL_LOADER_FACTORY_H_

#include "content/common/content_export.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/public/common/url_loader_factory.mojom.h"

namespace content {

class CONTENT_EXPORT CORSURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  static void CreateAndBind(PossiblyAssociatedInterfacePtr<
                                mojom::URLLoaderFactory> network_loader_factory,
                            mojom::URLLoaderFactoryRequest request);

  explicit CORSURLLoaderFactory(
      PossiblyAssociatedInterfacePtr<mojom::URLLoaderFactory>
          network_loader_factory);

  ~CORSURLLoaderFactory() override;

  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;

  void SyncLoad(int32_t routing_id,
                int32_t request_id,
                const ResourceRequest& resource_request,
                SyncLoadCallback callback) override;

 private:
  PossiblyAssociatedInterfacePtr<mojom::URLLoaderFactory>
      network_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_CHILD_LOADER_CORS_URL_LOADER_FACTORY_H_

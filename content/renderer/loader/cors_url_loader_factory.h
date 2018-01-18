// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_CORS_URL_LOADER_FACTORY_H_
#define CONTENT_RENDERER_LOADER_CORS_URL_LOADER_FACTORY_H_

#include "content/common/content_export.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/common/possibly_associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace content {

// URLLoaderFactory that adds cross-origin resource sharing capabilities
// (https://www.w3.org/TR/cors/), delegating requests as well as potential
// preflight requests to the supplied |network_loader_factory|. Its lifetime is
// bound to that of the pipes connected to it and the CORSURLLoader instances it
// creates.
class CONTENT_EXPORT CORSURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  static void CreateAndBind(
      PossiblyAssociatedInterfacePtrInfo<network::mojom::URLLoaderFactory>
          network_loader_factory,
      network::mojom::URLLoaderFactoryRequest request);

  explicit CORSURLLoaderFactory(
      PossiblyAssociatedInterfacePtrInfo<network::mojom::URLLoaderFactory>
          network_loader_factory);
  ~CORSURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

  void OnConnectionError();

 private:
  PossiblyAssociatedInterfacePtr<network::mojom::URLLoaderFactory>
      network_loader_factory_;

  // The factory owns the CORSURLLoaders it creates so that they can share
  // |network_loader_factory_|.
  mojo::StrongBindingSet<network::mojom::URLLoader> loader_bindings_;

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_CORS_URL_LOADER_FACTORY_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_
#define CONTENT_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

// A factory class to create a URLLoader that supports CORS.
// This class takes a network::mojom::URLLoaderFactory instance in the
// constructor and owns it to make network requests for CORS preflight, and
// actual network request.
class CONTENT_EXPORT CORSURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  explicit CORSURLLoaderFactory(
      std::unique_ptr<network::mojom::URLLoaderFactory> network_loader_factory);
  ~CORSURLLoaderFactory() override;

 private:
  // Implements mojom::URLLoaderFactory.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  std::unique_ptr<network::mojom::URLLoaderFactory> network_loader_factory_;

  // The factory owns the CORSURLLoader it creates.
  mojo::StrongBindingSet<network::mojom::URLLoader> loader_bindings_;

  DISALLOW_COPY_AND_ASSIGN(CORSURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_CONTEXT_H_
#define CONTENT_NETWORK_NETWORK_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/network_service.mojom.h"
#include "content/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"

namespace net {
class URLRequestContext;
}

namespace content {
class URLLoaderImpl;

class NetworkContext : public mojom::NetworkContext {
 public:
  NetworkContext(mojom::NetworkContextRequest request,
                 mojom::NetworkContextParamsPtr params);
  ~NetworkContext() override;

  CONTENT_EXPORT static std::unique_ptr<NetworkContext> CreateForTesting();

  net::URLRequestContext* url_request_context() {
    return url_request_context_.get();
  }

  // These are called by individual url loaders as they are being created and
  // destroyed.
  void RegisterURLLoader(URLLoaderImpl* url_loader);
  void DeregisterURLLoader(URLLoaderImpl* url_loader);

  // mojom::NetworkContext implementation:
  void CreateURLLoaderFactory(mojom::URLLoaderFactoryRequest request,
                              uint32_t process_id) override;
  void HandleViewCacheRequest(const GURL& url,
                              mojom::URLLoaderClientPtr client) override;

 private:
  NetworkContext();

  std::unique_ptr<net::URLRequestContext> url_request_context_;

  // Put it below |url_request_context_| so that it outlives all the
  // NetworkServiceURLLoaderFactoryImpl instances.
  mojo::StrongBindingSet<mojom::URLLoaderFactory> loader_factory_bindings_;

  // URLLoaderImpls register themselves with the NetworkContext so that they can
  // be cleaned up when the NetworkContext goes away. This is needed as
  // net::URLRequests held by URLLoaderImpls have to be gone when
  // net::URLRequestContext (held by NetworkContext) is destroyed.
  std::set<URLLoaderImpl*> url_loaders_;

  // Set when entering the destructor, in order to avoid manipulations of the
  // |url_loaders_| (as a url_loader might delete itself in Cleanup()).
  bool in_shutdown_;

  mojom::NetworkContextParamsPtr params_;

  mojo::Binding<mojom::NetworkContext> binding_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContext);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_CONTEXT_H_

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
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"

namespace net {
class URLRequestContext;
class URLRequestContextBuilder;
}

namespace content {
class NetworkServiceImpl;
class URLLoaderImpl;

// A NetworkContext creates and manages access to a URLRequestContext.
//
// When the network service is enabled, NetworkContexts are created through
// NetworkService's mojo interface and are owned jointly by the NetworkService
// and the NetworkContextPtr used to talk to them, and the NetworkContext is
// destroyed when either one is torn down.
//
// When the network service is disabled, NetworkContexts may be created through
// NetworkServiceImpl::CreateNetworkContextWithBuilder, and take in a
// URLRequestContextBuilder to seed construction of the NetworkContext's
// URLRequestContext. When that happens, the consumer takes ownership of the
// NetworkContext directly, has direct access to its URLRequestContext, and is
// responsible for destroying it before the NetworkService.
class CONTENT_EXPORT NetworkContext : public mojom::NetworkContext {
 public:
  NetworkContext(NetworkServiceImpl* network_service,
                 mojom::NetworkContextRequest request,
                 mojom::NetworkContextParamsPtr params);

  // Temporary constructor that allows creating an in-process NetworkContext
  // with a pre-populated URLRequestContextBuilder.
  NetworkContext(mojom::NetworkContextRequest request,
                 mojom::NetworkContextParamsPtr params,
                 std::unique_ptr<net::URLRequestContextBuilder> builder);

  ~NetworkContext() override;

  static std::unique_ptr<NetworkContext> CreateForTesting();

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

  // Called when the associated NetworkServiceImpl is going away. Guaranteed to
  // destroy NetworkContext's URLRequestContext.
  void Cleanup();

 private:
  NetworkContext();

  // On connection errors the NetworkContext destroys itself.
  void OnConnectionError();

  NetworkServiceImpl* const network_service_;

  std::unique_ptr<net::URLRequestContext> url_request_context_;

  // Put it below |url_request_context_| so that it outlives all the
  // NetworkServiceURLLoaderFactoryImpl instances.
  mojo::StrongBindingSet<mojom::URLLoaderFactory> loader_factory_bindings_;

  // URLLoaderImpls register themselves with the NetworkContext so that they can
  // be cleaned up when the NetworkContext goes away. This is needed as
  // net::URLRequests held by URLLoaderImpls have to be gone when
  // net::URLRequestContext (held by NetworkContext) is destroyed.
  std::set<URLLoaderImpl*> url_loaders_;

  mojom::NetworkContextParamsPtr params_;

  mojo::Binding<mojom::NetworkContext> binding_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContext);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_CONTEXT_H_

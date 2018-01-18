// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_CONTEXT_H_
#define CONTENT_NETWORK_NETWORK_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/network/url_request_context_owner.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "services/network/cookie_manager.h"
#include "services/network/public/interfaces/network_service.mojom.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace net {
class CertVerifier;
class URLRequestContext;
class HttpServerPropertiesManager;
}

namespace content {
class NetworkServiceImpl;
class URLLoader;
class URLRequestContextBuilderMojo;

// A NetworkContext creates and manages access to a URLRequestContext.
//
// When the network service is enabled, NetworkContexts are created through
// NetworkService's mojo interface and are owned jointly by the NetworkService
// and the NetworkContextPtr used to talk to them, and the NetworkContext is
// destroyed when either one is torn down.
//
// When the network service is disabled, NetworkContexts may be created through
// NetworkServiceImpl::CreateNetworkContextWithBuilder, and take in a
// URLRequestContextBuilderMojo to seed construction of the NetworkContext's
// URLRequestContext. When that happens, the consumer takes ownership of the
// NetworkContext directly, has direct access to its URLRequestContext, and is
// responsible for destroying it before the NetworkService.
class CONTENT_EXPORT NetworkContext : public network::mojom::NetworkContext {
 public:
  NetworkContext(NetworkServiceImpl* network_service,
                 network::mojom::NetworkContextRequest request,
                 network::mojom::NetworkContextParamsPtr params);

  // Temporary constructor that allows creating an in-process NetworkContext
  // with a pre-populated URLRequestContextBuilderMojo.
  NetworkContext(NetworkServiceImpl* network_service,
                 network::mojom::NetworkContextRequest request,
                 network::mojom::NetworkContextParamsPtr params,
                 std::unique_ptr<URLRequestContextBuilderMojo> builder);

  // Creates a NetworkContext that wraps a consumer-provided URLRequestContext
  // that the NetworkContext does not own.
  // TODO(mmenke):  Remove this constructor when the network service ships.
  NetworkContext(NetworkServiceImpl* network_service,
                 network::mojom::NetworkContextRequest request,
                 net::URLRequestContext* url_request_context);

  ~NetworkContext() override;

  static std::unique_ptr<NetworkContext> CreateForTesting();

  // Sets a global CertVerifier to use when initializing all profiles.
  static void SetCertVerifierForTesting(net::CertVerifier* cert_verifier);

  net::URLRequestContext* url_request_context() { return url_request_context_; }

  NetworkServiceImpl* network_service() { return network_service_; }

  // These are called by individual url loaders as they are being created and
  // destroyed.
  void RegisterURLLoader(URLLoader* url_loader);
  void DeregisterURLLoader(URLLoader* url_loader);

  // network::mojom::NetworkContext implementation:
  void CreateURLLoaderFactory(network::mojom::URLLoaderFactoryRequest request,
                              uint32_t process_id) override;
  void HandleViewCacheRequest(
      const GURL& url,
      network::mojom::URLLoaderClientPtr client) override;
  void GetCookieManager(network::mojom::CookieManagerRequest request) override;
  void GetRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRequest request,
      int32_t render_process_id,
      int32_t render_frame_id) override;
  void ClearNetworkingHistorySince(
      base::Time time,
      base::OnceClosure completion_callback) override;
  void SetNetworkConditions(
      const std::string& profile_id,
      network::mojom::NetworkConditionsPtr conditions) override;
  void AddHSTSForTesting(const std::string& host,
                         base::Time expiry,
                         bool include_subdomains,
                         AddHSTSForTestingCallback callback) override;

  // Called when the associated NetworkServiceImpl is going away. Guaranteed to
  // destroy NetworkContext's URLRequestContext.
  void Cleanup();

  // Disables use of QUIC by the NetworkContext.
  void DisableQuic();


  // Applies the values in |network_context_params| to |builder|, and builds
  // the URLRequestContext.
  static URLRequestContextOwner ApplyContextParamsToBuilder(
      URLRequestContextBuilderMojo* builder,
      network::mojom::NetworkContextParams* network_context_params,
      bool quic_disabled,
      net::NetLog* net_log);

 private:
  // Constructor only used in tests.
  explicit NetworkContext(network::mojom::NetworkContextParamsPtr params);

  // On connection errors the NetworkContext destroys itself.
  void OnConnectionError();

  URLRequestContextOwner MakeURLRequestContext(
      network::mojom::NetworkContextParams* network_context_params);

  NetworkServiceImpl* const network_service_;

  // Holds owning pointer to |url_request_context_|. Will contain a nullptr for
  // |url_request_context| when the NetworkContextImpl doesn't own its own
  // URLRequestContext.
  URLRequestContextOwner url_request_context_owner_;

  net::URLRequestContext* url_request_context_ = nullptr;

  // Put it below |url_request_context_| so that it outlives all the
  // NetworkServiceURLLoaderFactory instances.
  mojo::StrongBindingSet<network::mojom::URLLoaderFactory>
      loader_factory_bindings_;

  // URLLoaders register themselves with the NetworkContext so that they can
  // be cleaned up when the NetworkContext goes away. This is needed as
  // net::URLRequests held by URLLoaders have to be gone when
  // net::URLRequestContext (held by NetworkContext) is destroyed.
  std::set<URLLoader*> url_loaders_;

  network::mojom::NetworkContextParamsPtr params_;

  mojo::Binding<network::mojom::NetworkContext> binding_;

  std::unique_ptr<network::CookieManager> cookie_manager_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContext);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_CONTEXT_H_

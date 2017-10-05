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
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/network/cookie_manager_impl.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"

class PrefService;

namespace net {
class URLRequestContext;
class URLRequestContextBuilder;
class HttpServerPropertiesManager;
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
  NetworkContext(NetworkServiceImpl* network_service,
                 mojom::NetworkContextRequest request,
                 mojom::NetworkContextParamsPtr params,
                 std::unique_ptr<net::URLRequestContextBuilder> builder);

  // Creates a NetworkContext that wraps a consumer-provided URLRequestContext
  // that the NetworkContext does not own. In this case, there is no
  // NetworkService object.
  // TODO(mmenke):  Remove this constructor when the network service ships.
  NetworkContext(mojom::NetworkContextRequest request,
                 net::URLRequestContext* url_request_context);

  ~NetworkContext() override;

  static std::unique_ptr<NetworkContext> CreateForTesting();

  net::URLRequestContext* url_request_context() { return url_request_context_; }

  NetworkServiceImpl* network_service() { return network_service_; }

  // These are called by individual url loaders as they are being created and
  // destroyed.
  void RegisterURLLoader(URLLoaderImpl* url_loader);
  void DeregisterURLLoader(URLLoaderImpl* url_loader);

  // mojom::NetworkContext implementation:
  void CreateURLLoaderFactory(mojom::URLLoaderFactoryRequest request,
                              uint32_t process_id) override;
  void HandleViewCacheRequest(const GURL& url,
                              mojom::URLLoaderClientPtr client) override;
  void GetCookieManager(mojom::CookieManagerRequest request) override;
  void ClearNetworkingHistorySince(
      base::Time time,
      base::OnceClosure completion_callback) override;

  // Called when the associated NetworkServiceImpl is going away. Guaranteed to
  // destroy NetworkContext's URLRequestContext.
  void Cleanup();

  // Disables use of QUIC by the NetworkContext.
  void DisableQuic();

 private:
  NetworkContext();

  // On connection errors the NetworkContext destroys itself.
  void OnConnectionError();

  std::unique_ptr<net::URLRequestContext> MakeURLRequestContext(
      mojom::NetworkContextParams* network_context_params);

  // Applies the values in |network_context_params| to |builder|.
  void ApplyContextParamsToBuilder(
      net::URLRequestContextBuilder* builder,
      mojom::NetworkContextParams* network_context_params);

  NetworkServiceImpl* const network_service_;

  // This needs to be destroyed after the URLRequestContext.
  std::unique_ptr<PrefService> pref_service_;

  // Owning pointer to |url_request_context_|. nullptr when the
  // NetworkContextImpl doesn't own its own URLRequestContext.
  std::unique_ptr<net::URLRequestContext> owned_url_request_context_;

  net::URLRequestContext* url_request_context_ = nullptr;

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

  std::unique_ptr<CookieManagerImpl> cookie_manager_;

  // Temporary class to help diagnose the impact of https://crbug.com/711579.
  // Every 24-hours, measures the size of the network cache and emits an UMA
  // metric.
  class DiskChecker {
   public:
    explicit DiskChecker(const base::FilePath& cache_path);
    ~DiskChecker();

   private:
    void CheckDiskSize();
    static void CheckDiskSizeOnBackgroundThread(
        const base::FilePath& cache_path);
    base::RepeatingTimer timer_;
    base::FilePath cache_path_;
    DISALLOW_COPY_AND_ASSIGN(DiskChecker);
  };
  std::unique_ptr<DiskChecker> disk_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContext);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_CONTEXT_H_

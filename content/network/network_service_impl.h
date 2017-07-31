// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_SERVICE_IMPL_H_
#define CONTENT_NETWORK_NETWORK_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/network/network_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace net {
class URLRequestContext;
class URLRequestContextBuilder;
}  // namespace net

namespace content {

class NetworkContext;

class CONTENT_EXPORT NetworkServiceImpl : public service_manager::Service,
                                          public NetworkService {
 public:
  explicit NetworkServiceImpl(
      std::unique_ptr<service_manager::BinderRegistry> registry);

  ~NetworkServiceImpl() override;

  std::unique_ptr<mojom::NetworkContext> CreateNetworkContextWithBuilder(
      content::mojom::NetworkContextRequest request,
      content::mojom::NetworkContextParamsPtr params,
      std::unique_ptr<net::URLRequestContextBuilder> builder,
      net::URLRequestContext** url_request_context) override;

  static std::unique_ptr<NetworkServiceImpl> CreateForTesting();

  // These are called by NetworkContexts as they are being created and
  // destroyed.
  void RegisterNetworkContext(NetworkContext* network_context);
  void DeregisterNetworkContext(NetworkContext* network_context);

  // mojom::NetworkService implementation:
  void CreateNetworkContext(mojom::NetworkContextRequest request,
                            mojom::NetworkContextParamsPtr params) override;

 private:
  class MojoNetLog;

  // service_manager::Service implementation.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void Create(mojom::NetworkServiceRequest request);

  std::unique_ptr<MojoNetLog> net_log_;

  std::unique_ptr<service_manager::BinderRegistry> registry_;

  mojo::Binding<mojom::NetworkService> binding_;

  // NetworkContexts register themselves with the NetworkService so that they
  // can be cleaned up when the NetworkService goes away. This is needed as
  // NetworkContexts share global state with the NetworkService, so must be
  // destroyed first.
  std::set<NetworkContext*> network_contexts_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceImpl);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_SERVICE_IMPL_H_

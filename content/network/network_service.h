// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_SERVICE_H_
#define CONTENT_NETWORK_NETWORK_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "content/common/network_service.mojom.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/network/network_context.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace content {

class NetworkService : public service_manager::Service,
                       public mojom::NetworkService {
 public:
  explicit NetworkService(
      std::unique_ptr<service_manager::BinderRegistry> registry);
  ~NetworkService() override;

 private:
  // service_manager::Service implementation.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void CreateURLLoaderFactory(
      const service_manager::BindSourceInfo& source_info,
      mojom::URLLoaderFactoryRequest request);
  void CreateNetworkService(const service_manager::BindSourceInfo& source_info,
                            mojom::NetworkServiceRequest request);

  // mojom::NetworkService implementation:
  void HandleViewCacheRequest(const ResourceRequest& request,
                              mojom::URLLoaderClientPtr client) override;

  std::unique_ptr<service_manager::BinderRegistry> registry_;

  NetworkContext context_;

  // Put it below |context_| so that |context_| outlives all the
  // NetworkServiceURLLoaderFactoryImpl instances.
  mojo::StrongBindingSet<mojom::URLLoaderFactory> loader_factory_bindings_;

  mojo::BindingSet<mojom::NetworkService> network_service_bindings_;

  DISALLOW_COPY_AND_ASSIGN(NetworkService);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_SERVICE_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_SERVICE_H_
#define CONTENT_NETWORK_NETWORK_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/network_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace content {

class NetworkContext;

class NetworkService : public service_manager::Service,
                       public mojom::NetworkService {
 public:
  explicit NetworkService(
      std::unique_ptr<service_manager::BinderRegistry> registry);
  ~NetworkService() override;

  CONTENT_EXPORT static std::unique_ptr<NetworkService> CreateForTesting();

  // These are called by NetworkContexts as they are being created and
  // destroyed.
  void RegisterNetworkContext(NetworkContext* network_context);
  void DeregisterNetworkContext(NetworkContext* network_context);

  // mojom::NetworkService implementation:
  void CreateNetworkContext(mojom::NetworkContextRequest request,
                            mojom::NetworkContextParamsPtr params) override;

 private:
  class MojoNetLog;

  // Used for tests.
  NetworkService();

  // service_manager::Service implementation.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void Create(const service_manager::BindSourceInfo& source_info,
              mojom::NetworkServiceRequest request);

  std::unique_ptr<MojoNetLog> net_log_;

  std::unique_ptr<service_manager::BinderRegistry> registry_;

  mojo::Binding<mojom::NetworkService> binding_;

  // NetworkContexts register themselves with the NetworkService so that they
  // can be cleaned up when the NetworkService goes away. This is needed as
  // NetworkContexts share global state with the NetworkService, so must be
  // destroyed first.
  std::set<NetworkContext*> network_contexts_;

  DISALLOW_COPY_AND_ASSIGN(NetworkService);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_SERVICE_H_

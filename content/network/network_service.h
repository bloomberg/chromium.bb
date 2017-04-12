// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_SERVICE_H_
#define CONTENT_NETWORK_NETWORK_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/network/network_context.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

namespace content {

class NetworkService
    : public service_manager::Service,
      public service_manager::InterfaceFactory<mojom::URLLoaderFactory> {
 public:
  NetworkService();
  ~NetworkService() override;

  static std::unique_ptr<service_manager::Service> CreateNetworkService();

 private:
  // service_manager::Service implementation.
  void OnBindInterface(const service_manager::ServiceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // service_manager::InterfaceFactory<mojom::UrlLoaderFactory>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::URLLoaderFactoryRequest request) override;

  service_manager::BinderRegistry registry_;

  NetworkContext context_;

  // Put it below |context_| so that |context_| outlives all the
  // NetworkServiceURLLoaderFactoryImpl instances.
  mojo::StrongBindingSet<mojom::URLLoaderFactory> loader_factory_bindings_;

  DISALLOW_COPY_AND_ASSIGN(NetworkService);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_SERVICE_H_

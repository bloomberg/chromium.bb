// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_SERVICE_H_
#define CONTENT_NETWORK_NETWORK_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "content/common/network_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
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
  class MojoNetLog;

  // service_manager::Service implementation.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void Create(const service_manager::BindSourceInfo& source_info,
              mojom::NetworkServiceRequest request);

  // mojom::NetworkService implementation:
  void CreateNetworkContext(mojom::NetworkContextRequest request,
                            mojom::NetworkContextParamsPtr params) override;

  std::unique_ptr<MojoNetLog> net_log_;

  std::unique_ptr<service_manager::BinderRegistry> registry_;

  mojo::Binding<mojom::NetworkService> binding_;

  DISALLOW_COPY_AND_ASSIGN(NetworkService);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_SERVICE_H_

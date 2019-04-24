// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_NETWORK_CONFIG_SERVICE_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_NETWORK_CONFIG_SERVICE_H_

#include <memory>

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace chromeos {

namespace network_config {

class CrosNetworkConfig;

// Service implementing mojom::CrosNetworkConfig.
class NetworkConfigService : public service_manager::Service {
 public:
  explicit NetworkConfigService(service_manager::mojom::ServiceRequest request);
  ~NetworkConfigService() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;
  std::unique_ptr<CrosNetworkConfig> cros_network_config_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigService);
};

}  // namespace network_config

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_NETWORK_CONFIG_SERVICE_H_

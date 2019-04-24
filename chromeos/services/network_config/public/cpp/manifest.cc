// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromeos {
namespace network_config {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Network Configuration Service")
          .WithOptions(service_manager::ManifestOptionsBuilder().Build())
          .ExposeCapability(mojom::kNetworkConfigCapability,
                            service_manager::Manifest::InterfaceList<
                                mojom::CrosNetworkConfig>())
          .Build()};
  return *manifest;
}

}  // namespace network_config
}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace quarantine {

const service_manager::Manifest& GetQuarantineManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Quarantine Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability(
              mojom::kQuarantineFileCapability,
              service_manager::Manifest::InterfaceList<mojom::Quarantine>())
          .Build()};
  return *manifest;
}

}  // namespace quarantine

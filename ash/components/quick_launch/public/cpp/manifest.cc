// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/quick_launch/public/cpp/manifest.h"

#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/ws/public/mojom/constants.mojom.h"

namespace quick_launch {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Quick Launch Bar")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .Build())
          .RequireCapability(ws::mojom::kServiceName, "app")
          .RequireCapability("catalog", "catalog:catalog")
          .RequireCapability("catalog", "directory")
          .RequireCapability("*", "mash:launchable")
          .Build()};
  return *manifest;
}
}  // namespace quick_launch

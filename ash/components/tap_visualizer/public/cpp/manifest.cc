// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/tap_visualizer/public/cpp/manifest.h"

#include "ash/components/tap_visualizer/public/mojom/tap_visualizer.mojom.h"
#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/ws/public/mojom/constants.mojom.h"

namespace tap_visualizer {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Show Taps")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .Build())
          .ExposeCapability(
              mojom::kShowUiCapability,
              service_manager::Manifest::InterfaceList<mojom::TapVisualizer>())
          .RequireCapability(ws::mojom::kServiceName, "app")
          .Build()};
  return *manifest;
}
}  // namespace tap_visualizer

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chrome/services/util_win/public/mojom/constants.mojom.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetUtilWinManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(chrome::mojom::kUtilWinServiceName)
          .WithDisplayName("Windows Utilities")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability("util_win",
                            service_manager::Manifest::InterfaceList<
                                chrome::mojom::UtilWin>())

          .Build()};
  return *manifest;
}

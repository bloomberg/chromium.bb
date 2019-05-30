// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/media_gallery_util/public/mojom/constants.mojom.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetMediaGalleryUtilManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(chrome::mojom::kMediaGalleryUtilServiceName)
          .WithDisplayName(IDS_UTILITY_PROCESS_MEDIA_GALLERY_UTILITY_NAME)
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("utility")
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability("parse_media",
                            service_manager::Manifest::InterfaceList<
                                chrome::mojom::MediaParserFactory>())
          .Build()};
  return *manifest;
}

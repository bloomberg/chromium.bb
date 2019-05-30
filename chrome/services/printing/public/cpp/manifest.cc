// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/printing/public/mojom/constants.mojom.h"
#include "chrome/services/printing/public/mojom/pdf_nup_converter.mojom.h"
#include "chrome/services/printing/public/mojom/pdf_to_pwg_raster_converter.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_WIN)
#include "chrome/services/printing/public/mojom/pdf_to_emf_converter.mojom.h"
#endif

const service_manager::Manifest& GetChromePrintingManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(printing::mojom::kChromePrintingServiceName)
        .WithDisplayName(IDS_UTILITY_PROCESS_PRINTING_SERVICE_NAME)
        .WithOptions(
            service_manager::ManifestOptionsBuilder()
                .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                       kOutOfProcessBuiltin)
                .WithSandboxType("utility")
                .WithInstanceSharingPolicy(
                    service_manager::Manifest::InstanceSharingPolicy::
                        kSharedAcrossGroups)
                .Build())
        .ExposeCapability("converter",
                          service_manager::Manifest::InterfaceList<
#if defined(OS_WIN)
                              printing::mojom::PdfToEmfConverterFactory,
#endif
                              printing::mojom::PdfNupConverter,
                              printing::mojom::PdfToPwgRasterConverter>())

        .Build()
  };
  return *manifest;
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_packaged_services_manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/services/heap_profiling/manifest.h"
#include "content/public/common/service_names.mojom.h"
#include "media/mojo/services/cdm_manifest.h"
#include "media/mojo/services/media_manifest.h"
#include "services/audio/manifest.h"
#include "services/data_decoder/manifest.h"
#include "services/device/manifest.h"
#include "services/media_session/manifest.h"
#include "services/metrics/manifest.h"
#include "services/network/manifest.h"
#include "services/resource_coordinator/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/shape_detection/manifest.h"
#include "services/tracing/manifest.h"
#include "services/video_capture/manifest.h"
#include "services/viz/manifest.h"

#if defined(OS_LINUX)
#include "components/services/font/manifest.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/assistant/buildflags.h"
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/services/assistant/audio_decoder/manifest.h"
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)

namespace content {

const service_manager::Manifest& GetContentPackagedServicesManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(mojom::kPackagedServicesServiceName)
        .WithOptions(service_manager::ManifestOptionsBuilder()
                         .WithInstanceSharingPolicy(
                             service_manager::Manifest::InstanceSharingPolicy::
                                 kSingleton)
                         .CanConnectToInstancesInAnyGroup(true)
                         .CanRegisterOtherServiceInstances(true)
                         .Build())
        .ExposeCapability("service_manager:service_factory",
                          std::set<const char*>{
                              "service_manager.mojom.ServiceFactory",
                          })
        .RequireCapability(mojom::kBrowserServiceName, "")
        .RequireCapability("*", "app")
        .PackageService(heap_profiling::GetManifest())
        .PackageService(cdm::GetManifest())
        .PackageService(media::GetManifest())
        .PackageService(audio::GetManifest())
        .PackageService(data_decoder::GetManifest())
        .PackageService(device::GetManifest())
        .PackageService(media_session::GetManifest())
        .PackageService(metrics::GetManifest())
        .PackageService(network::GetManifest())
        .PackageService(resource_coordinator::GetManifest())
        .PackageService(shape_detection::GetManifest())
        .PackageService(tracing::GetManifest())
        .PackageService(video_capture::GetManifest())
        .PackageService(viz::GetManifest())
#if defined(OS_LINUX)
        .PackageService(font_service::GetManifest())
#endif
#if defined(OS_CHROMEOS)
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
        .PackageService(assistant_audio_decoder::GetManifest())
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)
        .Build()
  };
  return *manifest;
}

}  // namespace content

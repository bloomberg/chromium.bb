// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/builtin_service_manifests.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/services/heap_profiling/public/cpp/manifest.h"
#include "content/public/app/content_browser_manifest.h"
#include "content/public/app/content_gpu_manifest.h"
#include "content/public/app/content_plugin_manifest.h"
#include "content/public/app/content_renderer_manifest.h"
#include "content/public/app/content_utility_manifest.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_names.mojom.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/services/cdm_manifest.h"
#include "media/mojo/services/media_manifest.h"
#include "services/audio/public/cpp/manifest.h"
#include "services/data_decoder/public/cpp/manifest.h"
#include "services/device/public/cpp/manifest.h"
#include "services/media_session/public/cpp/manifest.h"
#include "services/metrics/public/cpp/manifest.h"
#include "services/network/public/cpp/manifest.h"
#include "services/resource_coordinator/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/shape_detection/public/cpp/manifest.h"
#include "services/tracing/manifest.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/video_capture/public/cpp/manifest.h"
#include "services/viz/public/cpp/manifest.h"

#if defined(OS_LINUX)
#include "components/services/font/public/cpp/manifest.h"  // nogncheck
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/assistant/buildflags.h"
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/services/assistant/public/cpp/audio_decoder_manifest.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)

namespace content {

namespace {

bool IsAudioServiceOutOfProcess() {
  return base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) &&
         !GetContentClient()->browser()->OverridesAudioManager();
}

// TODO(https://crbug.com/781334): Remove these helpers and just update the
// manifest definitions to be marked out-of-process. This is here only to avoid
// extra churn when transitioning away from content_packaged_services.
service_manager::Manifest MakeOutOfProcess(
    const service_manager::Manifest& manifest) {
  service_manager::Manifest copy(manifest);
  copy.options.execution_mode =
      service_manager::Manifest::ExecutionMode::kOutOfProcessBuiltin;
  return copy;
}

template <typename Predicate>
service_manager::Manifest MakeOutOfProcessIf(
    Predicate predicate,
    const service_manager::Manifest& manifest) {
  if (predicate())
    return MakeOutOfProcess(manifest);
  return manifest;
}

}  // namespace

const std::vector<service_manager::Manifest>& GetBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{
      std::vector<service_manager::Manifest>{
          GetContentBrowserManifest(),

          // NOTE: Content child processes are of course not running in the
          // browser process, but the distinction between "in-process" and
          // "out-of-process" manifests is temporary. For now, this is the right
          // place for these manifests.
          GetContentGpuManifest(),
          GetContentPluginManifest(),
          GetContentRendererManifest(),
          GetContentUtilityManifest(),

          heap_profiling::GetManifest(),
          MakeOutOfProcessIf([] { return IsAudioServiceOutOfProcess(); },
                             audio::GetManifest()),

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
          MakeOutOfProcess(media::GetCdmManifest()),
#endif

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS) || \
    BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
          MakeOutOfProcess(media::GetMediaManifest()),
#else
          media::GetMediaManifest(),
#endif
          MakeOutOfProcess(data_decoder::GetManifest()),
          device::GetManifest(),
          media_session::GetManifest(),
          metrics::GetManifest(),
          MakeOutOfProcessIf([] { return !IsInProcessNetworkService(); },
                             network::GetManifest()),
          resource_coordinator::GetManifest(),
          MakeOutOfProcess(shape_detection::GetManifest()),
          MakeOutOfProcessIf(
              [] {
                return !base::FeatureList::IsEnabled(
                    features::kTracingServiceInProcess);
              },
              tracing::GetManifest()),
          MakeOutOfProcessIf(
              [] {
                return features::IsVideoCaptureServiceEnabledForOutOfProcess();
              },
              video_capture::GetManifest()),
          MakeOutOfProcess(viz::GetManifest()),
#if defined(OS_LINUX)
          font_service::GetManifest(),
#endif
#if defined(OS_CHROMEOS)
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
          // TODO(https://crbug.com/929340): This doesn't belong here!
          MakeOutOfProcess(chromeos::assistant::GetAudioDecoderManifest()),
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)
      }};
  return *manifests;
}

}  // namespace content

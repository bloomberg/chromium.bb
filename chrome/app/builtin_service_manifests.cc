// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/builtin_service_manifests.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/constants.mojom.h"
#include "chrome/services/file_util/public/cpp/manifest.h"
#include "chrome/services/noop/public/cpp/manifest.h"
#include "components/services/patch/public/cpp/manifest.h"
#include "components/services/quarantine/public/cpp/manifest.h"
#include "components/services/unzip/public/cpp/manifest.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/startup_metric_utils/common/startup_metric.mojom.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/preferences/public/cpp/local_state_manifest.h"
#include "services/proxy_resolver/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/manifest.h"
#include "chrome/services/cups_ipp_parser/public/cpp/manifest.h"  // nogncheck
#include "chrome/services/cups_proxy/public/cpp/manifest.h"
#include "chromeos/services/cellular_setup/public/cpp/manifest.h"
#include "chromeos/services/ime/public/cpp/manifest.h"
#include "chromeos/services/network_config/public/cpp/manifest.h"
#include "chromeos/services/secure_channel/public/cpp/manifest.h"
#include "services/ws/public/mojom/input_devices/input_device_controller.mojom.h"
#endif

#if defined(OS_MACOSX)
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#endif

#if defined(OS_WIN)
#include "base/feature_list.h"
#include "chrome/services/util_win/public/cpp/manifest.h"
#include "chrome/services/wifi_util_win/public/cpp/manifest.h"
#include "components/services/quarantine/quarantine_features_win.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/utility/importer/profile_import_manifest.h"
#include "components/mirroring/service/manifest.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/services/removable_storage_writer/public/cpp/manifest.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
#include "chrome/services/media_gallery_util/public/cpp/manifest.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/services/pdf_compositor/public/cpp/manifest.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/services/printing/public/cpp/manifest.h"
#endif

#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
#include "chrome/services/isolated_xr_device/manifest.h"
#endif

#if BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_IN_PROCESS) || \
    BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_OUT_OF_PROCESS)
#include "services/content/simple_browser/public/cpp/manifest.h"  // nogncheck
#endif

namespace {

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

const service_manager::Manifest& GetChromeManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(chrome::mojom::kServiceName)
        .WithDisplayName("Chrome")
        .WithOptions(service_manager::ManifestOptionsBuilder()
                         .WithInstanceSharingPolicy(
                             service_manager::Manifest::InstanceSharingPolicy::
                                 kSharedAcrossGroups)
                         .CanConnectToInstancesWithAnyId(true)
                         .CanRegisterOtherServiceInstances(true)
                         .Build())
        .ExposeCapability("renderer",
                          service_manager::Manifest::InterfaceList<
#if defined(OS_MACOSX)
                              spellcheck::mojom::SpellCheckPanelHost,
#endif
                              spellcheck::mojom::SpellCheckHost,
                              startup_metric_utils::mojom::StartupMetricHost>())
#if defined(OS_CHROMEOS)
        // Only used in the classic Ash case.
        .ExposeCapability("input_device_controller",
                          service_manager::Manifest::InterfaceList<
                              ws::mojom::InputDeviceController>())
#endif
        .RequireCapability(chrome::mojom::kRendererServiceName, "browser")
        .Build()
  };
  return *manifest;
}

#if defined(OS_ANDROID)
const service_manager::Manifest& GetAndroidDownloadManagerManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName("download_manager")
          .WithDisplayName("Download Manager")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSingleton)
                           .Build())
          .RequireCapability("network", "network_service")
          .RequireCapability("device", "device:wake_lock")
          .Build()};
  return *manifest;
}
#endif

}  // namespace

const std::vector<service_manager::Manifest>&
GetChromeBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{{
      GetChromeManifest(),
      MakeOutOfProcess(GetFileUtilManifest()),
      MakeOutOfProcess(GetNoopManifest()),
      MakeOutOfProcess(patch::GetManifest()),
      MakeOutOfProcess(unzip::GetManifest()),
      MakeOutOfProcess(proxy_resolver::GetManifest()),
      MakeOutOfProcess(prefs::GetLocalStateManifest()),
#if defined(OS_WIN)
      MakeOutOfProcessIf(
          [] {
            return base::FeatureList::IsEnabled(
                quarantine::kOutOfProcessQuarantine);
          },
          quarantine::GetQuarantineManifest()),
#else
      quarantine::GetQuarantineManifest(),
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
      MakeOutOfProcess(GetRemovableStorageWriterManifest()),
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_ANDROID)
      MakeOutOfProcess(GetMediaGalleryUtilManifest()),
#endif
#if BUILDFLAG(ENABLE_PRINTING)
      MakeOutOfProcess(printing::GetPdfCompositorManifest()),
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW) || \
    (BUILDFLAG(ENABLE_PRINTING) && defined(OS_WIN))
      MakeOutOfProcess(GetChromePrintingManifest()),
#endif
#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
      MakeOutOfProcess(GetXrDeviceServiceManifest()),
#endif
#if BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_IN_PROCESS)
      simple_browser::GetManifest(),
#elif BUILDFLAG(ENABLE_SIMPLE_BROWSER_SERVICE_OUT_OF_PROCESS)
      MakeOutOfProcess(simple_browser::GetManifest()),
#endif
#if defined(OS_WIN)
      MakeOutOfProcess(GetUtilWinManifest()),
      MakeOutOfProcess(GetWifiUtilWinManifest()),
#endif
#if defined(OS_ANDROID)
      GetAndroidDownloadManagerManifest(),
#else
      MakeOutOfProcess(mirroring::GetManifest()),
      MakeOutOfProcess(GetProfileImportManifest()),
#endif
#if defined(OS_CHROMEOS)
      ash::GetManifest(),
      MakeOutOfProcess(GetCupsIppParserManifest()),
      chromeos::cellular_setup::GetManifest(),
      MakeOutOfProcess(chromeos::printing::GetCupsProxyManifest()),
      MakeOutOfProcess(chromeos::ime::GetManifest()),
      chromeos::network_config::GetManifest(),
      chromeos::secure_channel::GetManifest(),
#endif
  }};
  return *manifests;
}

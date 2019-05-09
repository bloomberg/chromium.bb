// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_content_browser_client.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/manifest.h"
#include "ash/public/cpp/test_manifest.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "ash/shell/content/client/shell_browser_main_parts.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/content_utility_client.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/ws/ime/test_ime_driver/public/cpp/manifest.h"
#include "services/ws/ime/test_ime_driver/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/window_service.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ash {
namespace shell {

namespace {

const service_manager::Manifest& GetAshShellBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .RequireCapability(device::mojom::kServiceName, "device:fingerprint")
          .Build()};
  return *manifest;
}

const service_manager::Manifest& GetAshShellPackagedServicesOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .PackageService(service_manager::Manifest(ash::GetManifest())
                              .Amend(ash::GetManifestOverlayForTesting()))
          .PackageService(test_ime_driver::GetManifest())
          .Build()};
  return *manifest;
}

}  // namespace

ShellContentBrowserClient::ShellContentBrowserClient()
    : shell_browser_main_parts_(nullptr) {}

ShellContentBrowserClient::~ShellContentBrowserClient() = default;

content::BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  shell_browser_main_parts_ = new ShellBrowserMainParts(parameters);
  return shell_browser_main_parts_;
}

void ShellContentBrowserClient::GetQuotaSettings(
    content::BrowserContext* context,
    content::StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  storage::GetNominalDynamicSettings(
      partition->GetPath(), context->IsOffTheRecord(),
      storage::GetDefaultDiskInfoHelper(), std::move(callback));
}

base::Optional<service_manager::Manifest>
ShellContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  // This is necessary for outgoing interface requests.
  if (name == content::mojom::kBrowserServiceName)
    return GetAshShellBrowserOverlayManifest();

  if (name == content::mojom::kPackagedServicesServiceName)
    return GetAshShellPackagedServicesOverlayManifest();

  return base::nullopt;
}

void ShellContentBrowserClient::RegisterOutOfProcessServices(
    OutOfProcessServiceMap* services) {
  (*services)[test_ime_driver::mojom::kServiceName] = base::BindRepeating(
      &base::ASCIIToUTF16, test_ime_driver::mojom::kServiceName);
}

}  // namespace shell
}  // namespace ash

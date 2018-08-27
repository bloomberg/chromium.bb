// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_content_browser_client.h"

#include <utility>

#include "ash/ash_service.h"
#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "ash/components/tap_visualizer/public/mojom/constants.mojom.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "ash/shell/content/client/shell_browser_main_parts.h"
#include "ash/shell/grit/ash_shell_resources.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/content_utility_client.h"
#include "services/ws/ime/test_ime_driver/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/window_service.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace shell {

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
      partition->GetPath(), context->IsOffTheRecord(), std::move(callback));
}

std::unique_ptr<base::Value>
ShellContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == content::mojom::kBrowserServiceName) {
    // This is necessary for outgoing interface requests (such as the keyboard
    // shortcut viewer).
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    base::StringPiece manifest_contents = rb.GetRawDataResourceForScale(
        IDR_ASH_SHELL_CONTENT_BROWSER_MANIFEST_OVERLAY,
        ui::ScaleFactor::SCALE_FACTOR_NONE);
    return base::JSONReader::Read(manifest_contents);
  }
  if (name == content::mojom::kPackagedServicesServiceName) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    base::StringPiece manifest_contents = rb.GetRawDataResourceForScale(
        IDR_ASH_SHELL_CONTENT_PACKAGED_SERVICES_MANIFEST_OVERLAY,
        ui::ScaleFactor::SCALE_FACTOR_NONE);
    return base::JSONReader::Read(manifest_contents);
  }
  return nullptr;
}

std::vector<content::ContentBrowserClient::ServiceManifestInfo>
ShellContentBrowserClient::GetExtraServiceManifests() {
  return {
      {quick_launch::mojom::kServiceName, IDR_ASH_SHELL_QUICK_LAUNCH_MANIFEST},
      {shortcut_viewer::mojom::kServiceName,
       IDR_ASH_SHELL_SHORTCUT_VIEWER_MANIFEST},
      {tap_visualizer::mojom::kServiceName,
       IDR_ASH_SHELL_TAP_VISUALIZER_MANIFEST},
      {test_ime_driver::mojom::kServiceName,
       IDR_ASH_SHELL_TEST_IME_DRIVER_MANIFEST},
  };
}

void ShellContentBrowserClient::RegisterOutOfProcessServices(
    OutOfProcessServiceMap* services) {
  (*services)[quick_launch::mojom::kServiceName] = base::BindRepeating(
      &base::ASCIIToUTF16, quick_launch::mojom::kServiceName);
  (*services)[shortcut_viewer::mojom::kServiceName] = base::BindRepeating(
      &base::ASCIIToUTF16, shortcut_viewer::mojom::kServiceName);
  (*services)[tap_visualizer::mojom::kServiceName] = base::BindRepeating(
      &base::ASCIIToUTF16, tap_visualizer::mojom::kServiceName);
  (*services)[test_ime_driver::mojom::kServiceName] = base::BindRepeating(
      &base::ASCIIToUTF16, test_ime_driver::mojom::kServiceName);
}

void ShellContentBrowserClient::RegisterInProcessServices(
    StaticServiceMap* services,
    content::ServiceManagerConnection* connection) {
  services->insert(std::make_pair(mojom::kServiceName,
                                  AshService::CreateEmbeddedServiceInfo()));
}

}  // namespace shell
}  // namespace ash

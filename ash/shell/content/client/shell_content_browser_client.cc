// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_content_browser_client.h"

#include <utility>

#include "ash/ash_service.h"
#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "ash/content/content_gpu_support.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "ash/shell/content/client/shell_browser_main_parts.h"
#include "ash/shell/grit/ash_shell_resources.h"
#include "ash/ws/window_service_delegate_impl.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/services/font/public/interfaces/constants.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/content_utility_client.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_service_delegate.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"

namespace ash {
namespace shell {
namespace {

std::unique_ptr<service_manager::Service> CreateWindowService() {
  return std::make_unique<ui::ws2::WindowService>(
      Shell::Get()->window_service_delegate(),
      std::make_unique<ContentGpuSupport>());
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
      partition->GetPath(), context->IsOffTheRecord(), std::move(callback));
}

std::unique_ptr<base::Value>
ShellContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (name != content::mojom::kPackagedServicesServiceName)
    return nullptr;

  base::StringPiece manifest_contents = rb.GetRawDataResourceForScale(
      IDR_ASH_SHELL_CONTENT_PACKAGED_SERVICES_MANIFEST_OVERLAY,
      ui::ScaleFactor::SCALE_FACTOR_NONE);
  return base::JSONReader::Read(manifest_contents);
}

std::vector<content::ContentBrowserClient::ServiceManifestInfo>
ShellContentBrowserClient::GetExtraServiceManifests() {
  return {
      {quick_launch::mojom::kServiceName, IDR_ASH_SHELL_QUICK_LAUNCH_MANIFEST},
      {font_service::mojom::kServiceName, IDR_ASH_SHELL_FONT_SERVICE_MANIFEST}};
}

void ShellContentBrowserClient::RegisterOutOfProcessServices(
    OutOfProcessServiceMap* services) {
  (*services)[quick_launch::mojom::kServiceName] = OutOfProcessServiceInfo(
      base::ASCIIToUTF16(quick_launch::mojom::kServiceName));
  (*services)[font_service::mojom::kServiceName] = OutOfProcessServiceInfo(
      base::ASCIIToUTF16(font_service::mojom::kServiceName));
}

void ShellContentBrowserClient::RegisterInProcessServices(
    StaticServiceMap* services) {
  services->insert(std::make_pair(ash::mojom::kServiceName,
                                  AshService::CreateEmbeddedServiceInfo()));

  service_manager::EmbeddedServiceInfo ws_service_info;
  ws_service_info.factory = base::BindRepeating(&CreateWindowService);
  ws_service_info.task_runner = base::ThreadTaskRunnerHandle::Get();
  services->insert(std::make_pair(ui::mojom::kServiceName, ws_service_info));
}

void ShellContentBrowserClient::AdjustUtilityServiceProcessCommandLine(
    const service_manager::Identity& identity,
    base::CommandLine* command_line) {
  if (identity.name() == quick_launch::mojom::kServiceName) {
    // TODO(sky): this is necessary because WindowTreeClient only connects to
    // the gpu related interfaces if Mash is set.
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    features::kMash.name);
  }
}

}  // namespace shell
}  // namespace ash

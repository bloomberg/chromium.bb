// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {

app_management::mojom::AppPtr CreateUIAppPtr(const apps::AppUpdate& update) {
  base::flat_map<uint32_t, apps::mojom::PermissionPtr> permissions;
  for (const auto& permission : update.Permissions()) {
    permissions[permission->permission_id] = permission->Clone();
  }

  return app_management::mojom::App::New(
      update.AppId(), update.AppType(), update.Name(),
      base::nullopt /*description*/,
      apps::mojom::OptionalBool::kUnknown /*is_pinned*/,
      base::nullopt /*version*/, base::nullopt /*size*/,
      std::move(permissions));
}

app_management::mojom::ExtensionAppPermissionMessagePtr
CreateExtensionAppPermissionMessage(
    const extensions::PermissionMessage& message) {
  std::vector<std::string> submessages;
  for (const auto& submessage : message.submessages()) {
    submessages.push_back(base::UTF16ToUTF8(submessage));
  }
  return app_management::mojom::ExtensionAppPermissionMessage::New(
      base::UTF16ToUTF8(message.message()), std::move(submessages));
}
}  // namespace

AppManagementPageHandler::AppManagementPageHandler(
    app_management::mojom::PageHandlerRequest request,
    app_management::mojom::PagePtr page,
    content::WebUI* web_ui)
    : binding_(this, std::move(request)),
      page_(std::move(page)),
      profile_(Profile::FromWebUI(web_ui)) {}

AppManagementPageHandler::~AppManagementPageHandler() {}

void AppManagementPageHandler::GetApps(GetAppsCallback callback) {
  apps::AppServiceProxy* proxy = apps::AppServiceProxy::Get(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;

  std::vector<app_management::mojom::AppPtr> apps;
  proxy->Cache().ForEachApp([&apps](const apps::AppUpdate& update) {
    apps.push_back(CreateUIAppPtr(update));
  });

  Observe(&proxy->Cache());

  std::move(callback).Run(std::move(apps));
}

void AppManagementPageHandler::SetPermission(
    const std::string& app_id,
    apps::mojom::PermissionPtr permission) {
  apps::AppServiceProxy* proxy = apps::AppServiceProxy::Get(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;

  proxy->SetPermission(app_id, std::move(permission));
}

void AppManagementPageHandler::Uninstall(const std::string& app_id) {
  apps::AppServiceProxy* proxy = apps::AppServiceProxy::Get(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;

  proxy->Uninstall(app_id);
}

void AppManagementPageHandler::OpenNativeSettings(const std::string& app_id) {
  apps::AppServiceProxy* proxy = apps::AppServiceProxy::Get(profile_);

  // TODO(crbug.com/826982): revisit pending decision on AppServiceProxy in
  // incognito
  if (!proxy)
    return;
  proxy->OpenNativeSettings(app_id);
}

void AppManagementPageHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.ReadinessChanged() &&
      update.Readiness() == apps::mojom::Readiness::kUninstalledByUser) {
    page_->OnAppRemoved(update.AppId());

  } else if (update.ReadinessChanged() &&
             update.Readiness() == apps::mojom::Readiness::kReady) {
    page_->OnAppAdded(CreateUIAppPtr(update));
  } else {
    page_->OnAppChanged(CreateUIAppPtr(update));
  }
}

void AppManagementPageHandler::GetExtensionAppPermissionMessages(
    const std::string& app_id,
    GetExtensionAppPermissionMessagesCallback callback) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const extensions::Extension* extension = registry->GetExtensionById(
      app_id, extensions::ExtensionRegistry::ENABLED |
                  extensions::ExtensionRegistry::DISABLED |
                  extensions::ExtensionRegistry::BLACKLISTED);
  std::vector<app_management::mojom::ExtensionAppPermissionMessagePtr> messages;
  if (extension) {
    for (const auto& message :
         extension->permissions_data()->GetPermissionMessages()) {
      messages.push_back(CreateExtensionAppPermissionMessage(message));
    }
  }
  std::move(callback).Run(std::move(messages));
}

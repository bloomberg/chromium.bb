// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace {

app_management::mojom::AppPtr CreateUIAppPtr(const apps::AppUpdate& update) {
  return app_management::mojom::App::New(
      update.AppId(), update.AppType(), update.Name(),
      base::nullopt /*description*/,
      apps::mojom::OptionalBool::kUnknown /*is_pinned*/,
      base::nullopt /*version*/, base::nullopt /*size*/);
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

void AppManagementPageHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.ReadinessChanged() &&
      update.Readiness() == apps::mojom::Readiness::kUninstalledByUser) {
    page_->OnAppRemoved(update.AppId());
    return;
  }

  if (update.ReadinessChanged() &&
      update.Readiness() == apps::mojom::Readiness::kReady) {
    page_->OnAppAdded(CreateUIAppPtr(update));
  } else {
    page_->OnAppChanged(CreateUIAppPtr(update));
  }
}

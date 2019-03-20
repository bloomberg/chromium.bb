// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/app_controller_impl.h"

#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace kiosk_next_home {

AppControllerImpl::AppControllerImpl(Profile* profile)
    : app_service_proxy_(apps::AppServiceProxy::Get(profile)) {}

AppControllerImpl::~AppControllerImpl() = default;

void AppControllerImpl::BindRequest(mojom::AppControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AppControllerImpl::GetApps(
    mojom::AppController::GetAppsCallback callback) {
  std::vector<chromeos::kiosk_next_home::mojom::AppPtr> app_list;
  // Using AppUpdate objects here since that's how the app list is intended to
  // be consumed. Refer to AppRegistryCache::ForEachApp for more information.
  app_service_proxy_->AppRegistryCache().ForEachApp(
      [this, &app_list](const apps::AppUpdate& update) {
        app_list.push_back(CreateAppPtr(update));
      });
  std::move(callback).Run(std::move(app_list));
}

void AppControllerImpl::LaunchApp(const std::string& app_id) {
  // TODO(ltenorio): Create a new launch source for Kiosk Next Home and use it
  // here.
  app_service_proxy_->Launch(app_id, ui::EventFlags::EF_NONE,
                             apps::mojom::LaunchSource::kFromAppListGrid,
                             display::kDefaultDisplayId);
}

chromeos::kiosk_next_home::mojom::AppPtr AppControllerImpl::CreateAppPtr(
    const apps::AppUpdate& update) {
  auto app = chromeos::kiosk_next_home::mojom::App::New();
  app->app_id = update.AppId();
  app->type = update.AppType();
  app->display_name = update.Name();
  app->readiness = update.Readiness();
  return app;
}

}  // namespace kiosk_next_home
}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/app_controller_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace chromeos {
namespace kiosk_next_home {

AppControllerImpl::AppControllerImpl(Profile* profile)
    : profile_(profile),
      app_service_proxy_(apps::AppServiceProxy::Get(profile)),
      url_prefix_(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kKioskNextHomeUrlPrefix)) {
  app_service_proxy_->AppRegistryCache().AddObserver(this);
}

AppControllerImpl::~AppControllerImpl() {
  // We can outlive the AppServiceProxy during Profile destruction, so we need
  // to test if the proxy still exists before removing the observer.
  if (apps::AppServiceProxy::Get(profile_))
    app_service_proxy_->AppRegistryCache().RemoveObserver(this);
}

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

void AppControllerImpl::SetClient(mojom::AppControllerClientPtr client) {
  client_ = std::move(client);
}

void AppControllerImpl::LaunchApp(const std::string& app_id) {
  // TODO(ltenorio): Create a new launch source for Kiosk Next Home and use it
  // here.
  app_service_proxy_->Launch(app_id, ui::EventFlags::EF_NONE,
                             apps::mojom::LaunchSource::kFromAppListGrid,
                             display::kDefaultDisplayId);
}

void AppControllerImpl::GetArcAndroidId(
    mojom::AppController::GetArcAndroidIdCallback callback) {
  arc::GetAndroidId(base::BindOnce(
      [](mojom::AppController::GetArcAndroidIdCallback callback, bool success,
         int64_t android_id) {
        // We need the string version of the Android ID since the int64_t
        // is too big for Javascript.
        std::move(callback).Run(success, base::NumberToString(android_id));
      },
      std::move(callback)));
}

void AppControllerImpl::LaunchHomeUrl(const std::string& suffix,
                                      LaunchHomeUrlCallback callback) {
  if (url_prefix_.empty()) {
    std::move(callback).Run(false, "No URL prefix.");
    return;
  }

  GURL url(url_prefix_ + suffix);
  if (!url.is_valid()) {
    std::move(callback).Run(false, "Invalid URL.");
    return;
  }

  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                LaunchIntent)
          : nullptr;

  if (!app_instance) {
    std::move(callback).Run(false, "ARC bridge not available.");
    return;
  }

  app_instance->LaunchIntent(url.spec(), display::kDefaultDisplayId);
  std::move(callback).Run(true, base::nullopt);
}

void AppControllerImpl::OnAppUpdate(const apps::AppUpdate& update) {
  // Skip this event if no relevant fields have changed.
  if (!update.StateIsNull() && !update.NameChanged() &&
      !update.ReadinessChanged()) {
    return;
  }

  if (client_) {
    client_->OnAppChanged(CreateAppPtr(update));
  }
}

mojom::AppPtr AppControllerImpl::CreateAppPtr(const apps::AppUpdate& update) {
  auto app = chromeos::kiosk_next_home::mojom::App::New();
  app->app_id = update.AppId();
  app->type = update.AppType();
  app->display_name = update.Name();
  app->readiness = update.Readiness();

  if (app->type == apps::mojom::AppType::kArc) {
    app->android_package_name = MaybeGetAndroidPackageName(app->app_id);
  }
  return app;
}

const std::string& AppControllerImpl::MaybeGetAndroidPackageName(
    const std::string& app_id) {
  // Try to find a cached package name for this app.
  const auto& package_name_it = android_package_map_.find(app_id);
  if (package_name_it != android_package_map_.end()) {
    return package_name_it->second;
  }

  // If we don't find it, try to get the package name from ARC prefs.
  ArcAppListPrefs* arc_prefs_ = ArcAppListPrefs::Get(profile_);
  if (!arc_prefs_) {
    return base::EmptyString();
  }
  std::unique_ptr<ArcAppListPrefs::AppInfo> arc_info =
      arc_prefs_->GetApp(app_id);
  if (!arc_info) {
    return base::EmptyString();
  }

  // Now that we have a valid package name, update our caches.
  android_package_map_[app_id] = arc_info->package_name;
  return android_package_map_[app_id];
}

}  // namespace kiosk_next_home
}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/crostini_apps.h"

#include <utility>

#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/apps/app_service/launch_util.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/grit/chrome_unscaled_resources.h"

// TODO(crbug.com/826982): the equivalent of
// CrostiniAppModelBuilder::MaybeCreateRootFolder. Does some sort of "root
// folder" abstraction belong here (on the publisher side of the App Service)
// or should we hard-code that in one particular subscriber (the App List UI)?

namespace apps {

CrostiniApps::CrostiniApps()
    : binding_(this), registry_(nullptr), next_u_key_(1) {}

CrostiniApps::~CrostiniApps() {
  if (registry_) {
    registry_->RemoveObserver(this);
  }
}

void CrostiniApps::Initialize(const apps::mojom::AppServicePtr& app_service,
                              Profile* profile) {
  if (!crostini::IsCrostiniUIAllowedForProfile(profile)) {
    return;
  }
  registry_ = crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  if (!registry_) {
    return;
  }

  registry_->AddObserver(this);

  apps::mojom::PublisherPtr publisher;
  binding_.Bind(mojo::MakeRequest(&publisher));
  app_service->RegisterPublisher(std::move(publisher),
                                 apps::mojom::AppType::kCrostini);
}

void CrostiniApps::Connect(apps::mojom::SubscriberPtr subscriber,
                           apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  for (const std::string& app_id : registry_->GetRegisteredAppIds()) {
    base::Optional<crostini::CrostiniRegistryService::Registration>
        registration = registry_->GetRegistration(app_id);
    if (registration.has_value()) {
      apps.push_back(Convert(app_id, *registration, true));
    }
  }
  subscriber->OnApps(std::move(apps));
  subscribers_.AddPtr(std::move(subscriber));
}

void CrostiniApps::LoadIcon(apps::mojom::IconKeyPtr icon_key,
                            apps::mojom::IconCompression icon_compression,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            LoadIconCallback callback) {
  if (!icon_key.is_null()) {
    if ((icon_key->icon_type == apps::mojom::IconType::kResource) &&
        (icon_key->u_key != 0) && (icon_key->u_key <= INT_MAX)) {
      int resource_id = static_cast<int>(icon_key->u_key);
      constexpr bool is_placeholder_icon = false;
      LoadIconFromResource(icon_compression, size_hint_in_dip, resource_id,
                           is_placeholder_icon, std::move(callback));
      return;
    }

    if (icon_key->icon_type == apps::mojom::IconType::kCrostini) {
      auto scale_factor = apps_util::GetPrimaryDisplayUIScaleFactor();

      // Try loading the icon from an on-disk cache. If that fails, fall back
      // to LoadIconFromVM.
      LoadIconFromFileWithFallback(
          icon_compression, size_hint_in_dip,
          registry_->GetIconPath(icon_key->s_key, scale_factor),
          std::move(callback),
          base::BindOnce(&CrostiniApps::LoadIconFromVM,
                         weak_ptr_factory_.GetWeakPtr(), icon_key->s_key,
                         icon_compression, size_hint_in_dip,
                         allow_placeholder_icon, scale_factor));
      return;
    }
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void CrostiniApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          int64_t display_id) {
  apps_util::Launch(app_id, event_flags, launch_source, display_id);
}

void CrostiniApps::SetPermission(const std::string& app_id,
                                 apps::mojom::PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void CrostiniApps::Uninstall(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void CrostiniApps::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void CrostiniApps::OnRegistryUpdated(
    crostini::CrostiniRegistryService* registry_service,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  for (const std::string& app_id : updated_apps) {
    PublishAppID(app_id, PublishAppIDType::kUpdate);
  }
  for (const std::string& app_id : removed_apps) {
    PublishAppID(app_id, PublishAppIDType::kUninstall);
  }
  for (const std::string& app_id : inserted_apps) {
    PublishAppID(app_id, PublishAppIDType::kInstall);
  }
}

void CrostiniApps::OnAppIconUpdated(const std::string& app_id,
                                    ui::ScaleFactor scale_factor) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kCrostini;
  app->app_id = app_id;
  app->icon_key = NewIconKey(app_id);
  Publish(std::move(app));
}

void CrostiniApps::LoadIconFromVM(const std::string icon_key_s_key,
                                  apps::mojom::IconCompression icon_compression,
                                  int32_t size_hint_in_dip,
                                  bool allow_placeholder_icon,
                                  ui::ScaleFactor scale_factor,
                                  LoadIconCallback callback) {
  if (!allow_placeholder_icon) {
    // Treat this as failure. We still run the callback, with the zero
    // IconValue.
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  // Provide a placeholder icon.
  constexpr bool is_placeholder_icon = true;
  LoadIconFromResource(icon_compression, size_hint_in_dip,
                       IDR_LOGO_CROSTINI_DEFAULT_192, is_placeholder_icon,
                       std::move(callback));

  // Ask the VM to load the icon (and write a cached copy to the file system).
  // The "Maybe" is because multiple requests for the same icon will be merged,
  // calling OnAppIconUpdated only once. In OnAppIconUpdated, we'll publish a
  // new IconKey, and subscribers can re-schedule new LoadIcon calls, with new
  // LoadIconCallback's, that will pick up that cached copy.
  //
  // TODO(crbug.com/826982): add a safeguard to prevent an infinite loop where
  // OnAppIconUpdated somehow doesn't write the cached icon file where we
  // expect, leading to another MaybeRequestIcon call, leading to another
  // OnAppIconUpdated call, leading to another MaybeRequestIcon call, etc.
  registry_->MaybeRequestIcon(icon_key_s_key, scale_factor);
}

apps::mojom::AppPtr CrostiniApps::Convert(
    const std::string& app_id,
    const crostini::CrostiniRegistryService::Registration& registration,
    bool new_icon_key) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kCrostini;
  app->app_id = app_id;
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = registration.Name();
  app->short_name = app->name;

  if (new_icon_key) {
    app->icon_key = NewIconKey(app_id);
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  app->installed_internally = apps::mojom::OptionalBool::kFalse;
  app->is_platform_app = apps::mojom::OptionalBool::kFalse;

  // TODO(crbug.com/826982): if Crostini isn't enabled, don't show the Terminal
  // item until it becomes enabled.
  auto show = !registration.NoDisplay() ? apps::mojom::OptionalBool::kTrue
                                        : apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_management = show;

  return app;
}

apps::mojom::IconKeyPtr CrostiniApps::NewIconKey(const std::string& app_id) {
  auto icon_key = apps::mojom::IconKey::New();

  // Treat the Crostini Terminal as a special case, loading an icon defined by
  // a resource instead of asking the Crostini VM (or the cache of previous
  // responses from the Crostini VM). Presumably this is for bootstrapping: the
  // Crostini Terminal icon (the UI for enabling and installing Crostini apps)
  // should be showable even before the user has installed their first Crostini
  // app and before bringing up an Crostini VM for the first time.
  if (app_id == crostini::kCrostiniTerminalId) {
    icon_key->icon_type = apps::mojom::IconType::kResource;
    icon_key->u_key = IDR_LOGO_CROSTINI_TERMINAL;
  } else {
    icon_key->icon_type = apps::mojom::IconType::kCrostini;
    icon_key->s_key = app_id;
    icon_key->u_key = next_u_key_++;
  }

  return icon_key;
}

void CrostiniApps::PublishAppID(const std::string& app_id,
                                PublishAppIDType type) {
  if (type == PublishAppIDType::kUninstall) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_type = apps::mojom::AppType::kCrostini;
    app->app_id = app_id;
    app->readiness = apps::mojom::Readiness::kUninstalledByUser;
    Publish(std::move(app));
    return;
  }

  base::Optional<crostini::CrostiniRegistryService::Registration> registration =
      registry_->GetRegistration(app_id);
  if (registration.has_value()) {
    Publish(Convert(app_id, *registration, type == PublishAppIDType::kInstall));
  }
}

void CrostiniApps::Publish(apps::mojom::AppPtr app) {
  subscribers_.ForAllPtrs([&app](apps::mojom::Subscriber* subscriber) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(app.Clone());
    subscriber->OnApps(std::move(apps));
  });
}

}  // namespace apps

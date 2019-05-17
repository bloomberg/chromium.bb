// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/crostini_apps.h"

#include <utility>

#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/apps/app_service/launch_util.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/prefs/pref_change_registrar.h"

// TODO(crbug.com/826982): the equivalent of
// CrostiniAppModelBuilder::MaybeCreateRootFolder. Does some sort of "root
// folder" abstraction belong here (on the publisher side of the App Service)
// or should we hard-code that in one particular subscriber (the App List UI)?

namespace apps {

CrostiniApps::CrostiniApps()
    : binding_(this),
      profile_(nullptr),
      registry_(nullptr),
      crostini_enabled_(false) {}

CrostiniApps::~CrostiniApps() {
  if (registry_) {
    registry_->RemoveObserver(this);
  }
}

void CrostiniApps::Initialize(const apps::mojom::AppServicePtr& app_service,
                              Profile* profile) {
  profile_ = nullptr;
  registry_ = nullptr;
  crostini_enabled_ = false;

  if (!crostini::IsCrostiniUIAllowedForProfile(profile)) {
    return;
  }
  registry_ = crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  if (!registry_) {
    return;
  }
  profile_ = profile;
  crostini_enabled_ = crostini::IsCrostiniEnabled(profile);

  registry_->AddObserver(this);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile->GetPrefs());
  pref_change_registrar_->Add(
      crostini::prefs::kCrostiniEnabled,
      base::BindRepeating(&CrostiniApps::OnCrostiniEnabledChanged,
                          base::Unretained(this)));

  apps::mojom::PublisherPtr publisher;
  binding_.Bind(mojo::MakeRequest(&publisher));
  app_service->RegisterPublisher(std::move(publisher),
                                 apps::mojom::AppType::kCrostini);
}

void CrostiniApps::ReInitializeForTesting(
    const apps::mojom::AppServicePtr& app_service,
    Profile* profile) {
  // Some test code creates a profile (and therefore profile-linked services
  // like the App Service) before it creates the fake user that lets
  // IsCrostiniUIAllowedForProfile return true. To work around that, we issue a
  // second Initialize call.
  Initialize(app_service, profile);
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

void CrostiniApps::LoadIcon(const std::string& app_id,
                            apps::mojom::IconKeyPtr icon_key,
                            apps::mojom::IconCompression icon_compression,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            LoadIconCallback callback) {
  if (icon_key) {
    if (icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId) {
      // The icon is a resource built into the Chrome OS binary.
      constexpr bool is_placeholder_icon = false;
      LoadIconFromResource(icon_compression, size_hint_in_dip,
                           icon_key->resource_id, is_placeholder_icon,
                           static_cast<IconEffects>(icon_key->icon_effects),
                           std::move(callback));
      return;
    } else {
      auto scale_factor = apps_util::GetPrimaryDisplayUIScaleFactor();

      // Try loading the icon from an on-disk cache. If that fails, fall back
      // to LoadIconFromVM.
      LoadIconFromFileWithFallback(
          icon_compression, size_hint_in_dip,
          registry_->GetIconPath(app_id, scale_factor),
          static_cast<IconEffects>(icon_key->icon_effects), std::move(callback),
          base::BindOnce(&CrostiniApps::LoadIconFromVM,
                         weak_ptr_factory_.GetWeakPtr(), app_id,
                         icon_compression, size_hint_in_dip,
                         allow_placeholder_icon, scale_factor,
                         static_cast<IconEffects>(icon_key->icon_effects)));
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

void CrostiniApps::OnCrostiniEnabledChanged() {
  crostini_enabled_ = profile_ && crostini::IsCrostiniEnabled(profile_);
  auto show = crostini_enabled_ ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;

  // The Crostini Terminal app is a hard-coded special case. It is the entry
  // point to installing other Crostini apps.
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kCrostini;
  app->app_id = crostini::kCrostiniTerminalId;
  app->show_in_launcher = show;
  app->show_in_search = show;
  Publish(std::move(app));
}

void CrostiniApps::LoadIconFromVM(const std::string app_id,
                                  apps::mojom::IconCompression icon_compression,
                                  int32_t size_hint_in_dip,
                                  bool allow_placeholder_icon,
                                  ui::ScaleFactor scale_factor,
                                  IconEffects icon_effects,
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
                       icon_effects, std::move(callback));

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
  registry_->MaybeRequestIcon(app_id, scale_factor);
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

  const std::string& executable_file_name = registration.ExecutableFileName();
  if (!executable_file_name.empty()) {
    app->additional_search_terms.push_back(executable_file_name);
  }
  for (const std::string& keyword : registration.Keywords()) {
    app->additional_search_terms.push_back(keyword);
  }

  if (new_icon_key) {
    app->icon_key = NewIconKey(app_id);
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  // TODO(crbug.com/955937): Change after deciding how crostini apps will
  // show in App Management.
  app->install_source = apps::mojom::InstallSource::kUser;

  app->is_platform_app = apps::mojom::OptionalBool::kFalse;
  app->recommendable = apps::mojom::OptionalBool::kTrue;
  app->searchable = apps::mojom::OptionalBool::kTrue;

  auto show = !registration.NoDisplay() ? apps::mojom::OptionalBool::kTrue
                                        : apps::mojom::OptionalBool::kFalse;
  if (registration.is_terminal_app()) {
    show = crostini_enabled_ ? apps::mojom::OptionalBool::kTrue
                             : apps::mojom::OptionalBool::kFalse;
  }
  app->show_in_launcher = show;
  app->show_in_search = show;
  // TODO(crbug.com/955937): Enable once Crostini apps are managed inside App
  // Management.
  app->show_in_management = apps::mojom::OptionalBool::kFalse;

  return app;
}

apps::mojom::IconKeyPtr CrostiniApps::NewIconKey(const std::string& app_id) {
  DCHECK(!app_id.empty());

  // Treat the Crostini Terminal as a special case, loading an icon defined by
  // a resource instead of asking the Crostini VM (or the cache of previous
  // responses from the Crostini VM). Presumably this is for bootstrapping: the
  // Crostini Terminal icon (the UI for enabling and installing Crostini apps)
  // should be showable even before the user has installed their first Crostini
  // app and before bringing up an Crostini VM for the first time.
  if (app_id == crostini::kCrostiniTerminalId) {
    return apps::mojom::IconKey::New(
        apps::mojom::IconKey::kDoesNotChangeOverTime,
        IDR_LOGO_CROSTINI_TERMINAL, apps::IconEffects::kNone);
  }

  return icon_key_factory_.MakeIconKey(apps::IconEffects::kNone);
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

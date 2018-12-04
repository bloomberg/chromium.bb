// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/extension_apps.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/extension_registry.h"
#include "mojo/public/cpp/bindings/interface_request.h"

// TODO(crbug.com/826982): life cycle events. Extensions can be installed and
// uninstalled. ExtensionApps should implement extensions::InstallObserver and
// be able to show download progress in the UI, a la ExtensionAppModelBuilder.
// This might involve using an extensions::InstallTracker. It might also need
// the equivalent of a LauncherExtensionAppUpdater.

// TODO(crbug.com/826982): ExtensionAppItem's can be 'badged', which means that
// it's an extension app that has its Android analog installed. We should cater
// for that here.

// TODO(crbug.com/826982): do we also need to watch prefs, the same as
// ExtensionAppModelBuilder?

// TODO(crbug.com/826982): support the is_platform_app bit. We might not need
// to plumb this all the way through the Mojo methods, as AFAICT it's only used
// for populating the context menu, which is done on the app publisher side
// (i.e. in this C++ file) and not at all on the app subscriber side.

namespace {

ash::ShelfLaunchSource ConvertLaunchSource(
    apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
      return ash::LAUNCH_FROM_UNKNOWN;
    case apps::mojom::LaunchSource::kFromAppList:
      return ash::LAUNCH_FROM_APP_LIST;
    case apps::mojom::LaunchSource::kFromAppListSearch:
      return ash::LAUNCH_FROM_APP_LIST_SEARCH;
  }
}

}  // namespace

namespace apps {

ExtensionApps::ExtensionApps()
    : binding_(this), profile_(nullptr), next_u_key_(1), observer_(this) {}

ExtensionApps::~ExtensionApps() = default;

void ExtensionApps::Initialize(const apps::mojom::AppServicePtr& app_service,
                               Profile* profile) {
  apps::mojom::PublisherPtr publisher;
  binding_.Bind(mojo::MakeRequest(&publisher));
  app_service->RegisterPublisher(std::move(publisher),
                                 apps::mojom::AppType::kExtension);

  profile_ = profile;
  if (profile_) {
    observer_.Add(extensions::ExtensionRegistry::Get(profile_));
  }
}

void ExtensionApps::Connect(apps::mojom::SubscriberPtr subscriber,
                            apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  if (profile_) {
    // TODO(crbug.com/826982): consider disabled and terminated extensions, not
    // just enabled ones, as per AppListControllerDelegate::GetApps in
    // https://cs.chromium.org/chromium/src/chrome/browser/ui/app_list/app_list_controller_delegate.cc?g=0&l=193
    for (const auto& extension :
         extensions::ExtensionRegistry::Get(profile_)->enabled_extensions()) {
      if (extension->is_app()) {
        apps.push_back(Convert(extension.get()));
      }
    }
  }
  subscriber->OnApps(std::move(apps));
  subscribers_.AddPtr(std::move(subscriber));
}

void ExtensionApps::LoadIcon(const std::string& app_id,
                             apps::mojom::IconKeyPtr icon_key,
                             apps::mojom::IconCompression icon_compression,
                             int32_t size_hint_in_dip,
                             LoadIconCallback callback) {
  if (!icon_key.is_null() &&
      (icon_key->icon_type == apps::mojom::IconType::kExtension) &&
      !icon_key->s_key.empty()) {
    LoadIconFromExtension(icon_compression, size_hint_in_dip,
                          std::move(callback), profile_, icon_key->s_key);
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void ExtensionApps::Launch(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) {
  if (!profile_) {
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);
  if (!extension || !extensions::util::IsAppLaunchable(app_id, profile_) ||
      RunExtensionEnableFlow(app_id)) {
    return;
  }

  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
      break;
    case apps::mojom::LaunchSource::kFromAppList:
      extensions::RecordAppListMainLaunch(extension);
      break;
    case apps::mojom::LaunchSource::kFromAppListSearch:
      extensions::RecordAppListSearchLaunch(extension);
      break;
  }

  ChromeLauncherController::instance()->LaunchApp(
      ash::ShelfID(app_id), ConvertLaunchSource(launch_source), event_flags,
      display_id);
}

apps::mojom::AppPtr ExtensionApps::Convert(
    const extensions::Extension* extension) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kExtension;
  app->app_id = extension->id();
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = extension->name();

  app->icon_key = apps::mojom::IconKey::New();
  app->icon_key->icon_type = apps::mojom::IconType::kExtension;
  app->icon_key->s_key = extension->id();
  app->icon_key->u_key = next_u_key_++;

  app->show_in_launcher = app_list::ShouldShowInLauncher(extension, profile_)
                              ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
  return app;
}

bool ExtensionApps::RunExtensionEnableFlow(const std::string& app_id) {
  if (extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    return false;
  }

  // TODO(crbug.com/826982): run the extension enable flow, doing what
  // chrome/browser/ui/app_list/extension_app_item.h does, even if we don't do
  // it in exactly the same way.
  //
  // Re-using the ExtensionEnableFlow code is not entirely trivial. An
  // ExtensionEnableFlow is created for one particular app_id, the same way
  // that an ExtensionAppItem maps 1:1 to an app_id. In contrast, this class
  // (the ExtensionApps publisher) handles all app_id's, not just one.

  return true;
}

}  // namespace apps

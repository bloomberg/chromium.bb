// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/extension_apps.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/strings/string16.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
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

// Only supporting important permissions for now.
const ContentSettingsType kSupportedPermissionTypes[] = {
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
    CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
    CONTENT_SETTINGS_TYPE_GEOLOCATION,
    CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
};

ash::ShelfLaunchSource ConvertLaunchSource(
    apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
      return ash::LAUNCH_FROM_UNKNOWN;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      return ash::LAUNCH_FROM_APP_LIST;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
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
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_);
    ConvertVector(registry->enabled_extensions(),
                  apps::mojom::Readiness::kReady, &apps);
    ConvertVector(registry->disabled_extensions(),
                  apps::mojom::Readiness::kDisabledByUser, &apps);
    ConvertVector(registry->terminated_extensions(),
                  apps::mojom::Readiness::kTerminated, &apps);
    // blacklisted_extensions and blocked_extensions, corresponding to
    // kDisabledByBlacklist and kDisabledByPolicy, are deliberately ignored.
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
    LoadIconFromExtension(icon_compression, size_hint_in_dip, profile_,
                          icon_key->s_key, std::move(callback));
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
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      extensions::RecordAppListMainLaunch(extension);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      app_list::RecordHistogram(app_list::APP_SEARCH_RESULT);
      extensions::RecordAppListSearchLaunch(extension);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      break;
  }

  ChromeLauncherController::instance()->LaunchApp(
      ash::ShelfID(app_id), ConvertLaunchSource(launch_source), event_flags,
      display_id);
}

void ExtensionApps::SetPermission(const std::string& app_id,
                                  apps::mojom::PermissionPtr permission) {
  if (!profile_) {
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);

  if (!extension->from_bookmark()) {
    return;
  }

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  const GURL url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);

  ContentSettingsType permission_type =
      static_cast<ContentSettingsType>(permission->permission_id);
  if (!base::ContainsValue(kSupportedPermissionTypes, permission_type)) {
    return;
  }

  DCHECK_EQ(permission->value_type,
            apps::mojom::PermissionValueType::kTriState);
  ContentSetting permission_value = CONTENT_SETTING_DEFAULT;
  switch (static_cast<apps::mojom::TriState>(permission->value)) {
    case apps::mojom::TriState::kAllow:
      permission_value = CONTENT_SETTING_ALLOW;
      break;
    case apps::mojom::TriState::kAsk:
      permission_value = CONTENT_SETTING_ASK;
      break;
    case apps::mojom::TriState::kBlock:
      permission_value = CONTENT_SETTING_BLOCK;
      break;
    default:  // Return if value is invalid.
      return;
  }

  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, permission_type, std::string() /* resource identifier */,
      permission_value);
}

void ExtensionApps::Uninstall(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);

  if (!extension) {
    return;
  }

  // TODO(crbug.com/826982): The UninstallReason should eventually be passed
  // through from the subscriber. This might involve generalising uninstall
  // reason into an App Service concept.
  base::string16 error;
  bool uninstalled =
      extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->UninstallExtension(
              app_id,
              extensions::UninstallReason::UNINSTALL_REASON_USER_INITIATED,
              &error);

  if (!uninstalled) {
    LOG(ERROR) << "Couldn't uninstall app with id " << app_id << ". " << error;
  }
}

void ExtensionApps::OpenNativeSettings(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);

  if (!extension) {
    return;
  }

  if (extension->is_hosted_app()) {
    chrome::ShowSiteSettings(
        profile_, extensions::AppLaunchInfo::GetFullLaunchURL(extension));

  } else if (extension->ShouldDisplayInExtensionSettings()) {
    Browser* browser = chrome::FindTabbedBrowser(profile_, false);
    if (browser) {
      chrome::ShowExtensions(browser, extension->id());
    }
    // TODO(crbug.com/826982): Either create new browser if one isn't found, or
    // make a version of chrome::ShowExtensions which accepts a Profile
    // instead of a Browser, similar to chrome::ShowSiteSettings.
  }
}

void ExtensionApps::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  if (IsBlacklisted(extension->id())) {
    return;
  }

  // TODO(crbug.com/826982): Does the is_update case need to be handled
  // differently? E.g. by only passing through fields that have changed.
  apps::mojom::AppPtr app = Convert(extension, apps::mojom::Readiness::kReady);

  subscribers_.ForAllPtrs([&app](apps::mojom::Subscriber* subscriber) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(app.Clone());
    subscriber->OnApps(std::move(apps));
  });
}

void ExtensionApps::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (IsBlacklisted(extension->id())) {
    return;
  }

  // Construct an App with only the information required to identify an
  // uninstallation.
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kExtension;
  app->app_id = extension->id();
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;

  subscribers_.ForAllPtrs([&app](apps::mojom::Subscriber* subscriber) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(app.Clone());
    subscriber->OnApps(std::move(apps));
  });
}

// static
bool ExtensionApps::IsBlacklisted(const std::string& app_id) {
  // We blacklist (meaning we don't publish the app, in the App Service sense)
  // some apps that are already published by other app publishers.
  //
  // This sense of "blacklist" is separate from the extension registry's
  // kDisabledByBlacklist concept, which is when SafeBrowsing will send out a
  // blacklist of malicious extensions to disable.

  // The Play Store is conceptually provided by the ARC++ publisher, but
  // because it (the Play Store icon) is also the UI for enabling Android apps,
  // we also want to show the app icon even before ARC++ is enabled. Prior to
  // the App Service, as a historical implementation quirk, the Play Store both
  // has an "ARC++ app" component and an "Extension app" component, and both
  // share the same App ID.
  //
  // In the App Service world, there should be a unique app publisher for any
  // given app. In this case, the ArcApps publisher publishes the Play Store
  // app, and the ExtensionApps publisher does not.
  return app_id == arc::kPlayStoreAppId;
}

apps::mojom::AppPtr ExtensionApps::Convert(
    const extensions::Extension* extension,
    apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kExtension;
  app->app_id = extension->id();
  app->readiness = readiness;
  app->name = extension->name();

  app->icon_key = apps::mojom::IconKey::New();
  app->icon_key->icon_type = apps::mojom::IconType::kExtension;
  app->icon_key->s_key = extension->id();
  app->icon_key->u_key = next_u_key_++;

  // Extensions where |from_bookmark| is true wrap websites and use web
  // permissions.
  if (extension->from_bookmark()) {
    const GURL url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    auto* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    DCHECK(host_content_settings_map);

    for (ContentSettingsType type : kSupportedPermissionTypes) {
      ContentSetting setting = host_content_settings_map->GetContentSetting(
          url, url, type, std::string() /* resource_identifier */);

      // Map ContentSettingsType to an apps::mojom::TriState value
      apps::mojom::TriState setting_val;
      switch (setting) {
        case CONTENT_SETTING_ALLOW:
          setting_val = apps::mojom::TriState::kAllow;
          break;
        case CONTENT_SETTING_ASK:
          setting_val = apps::mojom::TriState::kAsk;
          break;
        case CONTENT_SETTING_BLOCK:
          setting_val = apps::mojom::TriState::kBlock;
          break;
        default:
          setting_val = apps::mojom::TriState::kAsk;
      }

      auto permission = apps::mojom::Permission::New();
      permission->permission_id = static_cast<uint32_t>(type);
      permission->value_type = apps::mojom::PermissionValueType::kTriState;
      permission->value = static_cast<uint32_t>(setting_val);

      app->permissions.push_back(std::move(permission));
    }
  }

  // TODO(crbug.com/826982): does this catch default installed web apps?
  //
  // https://crrev.com/c/1377955/3/chrome/browser/apps/app_service/extension_apps.cc#263
  bool installed_internally =
      extension->was_installed_by_default() ||
      extension->was_installed_by_oem() ||
      extensions::Manifest::IsComponentLocation(extension->location()) ||
      extensions::Manifest::IsPolicyLocation(extension->location());
  app->installed_internally = installed_internally
                                  ? apps::mojom::OptionalBool::kTrue
                                  : apps::mojom::OptionalBool::kFalse;

  auto show = app_list::ShouldShowInLauncher(extension, profile_)
                  ? apps::mojom::OptionalBool::kTrue
                  : apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = show;
  app->show_in_search = show;

  return app;
}

void ExtensionApps::ConvertVector(const extensions::ExtensionSet& extensions,
                                  apps::mojom::Readiness readiness,
                                  std::vector<apps::mojom::AppPtr>* apps_out) {
  for (const auto& extension : extensions) {
    if (extension->is_app() && !IsBlacklisted(extension->id())) {
      apps_out->push_back(Convert(extension.get(), readiness));
    }
  }
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

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"

#include <utility>

#include "base/one_shot_event.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_display_mode_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/common/extensions/manifest_handlers/linked_app_icons.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

using web_app::DisplayMode;

namespace extensions {

BookmarkAppRegistrar::BookmarkAppRegistrar(Profile* profile)
    : AppRegistrar(profile) {
  extension_observer_.Add(ExtensionRegistry::Get(profile));
}

BookmarkAppRegistrar::~BookmarkAppRegistrar() = default;

bool BookmarkAppRegistrar::IsInstalled(const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  return extension && extension->from_bookmark();
}

bool BookmarkAppRegistrar::IsLocallyInstalled(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkApp(app_id);
  return extension && BookmarkAppIsLocallyInstalled(profile(), extension);
}

bool BookmarkAppRegistrar::WasInstalledByUser(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkApp(app_id);
  return extension && !extension->was_installed_by_default();
}

int BookmarkAppRegistrar::CountUserInstalledApps() const {
  return CountUserInstalledBookmarkApps(profile());
}

void BookmarkAppRegistrar::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  DCHECK_EQ(browser_context, profile());
  if (extension->from_bookmark())
    NotifyWebAppUninstalled(extension->id());
}

void BookmarkAppRegistrar::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK_EQ(browser_context, profile());
  if (!extension->from_bookmark())
    return;
  // If a profile is removed, notify the web app that it is uninstalled, so it
  // can cleanup any state outside the profile dir (e.g., registry settings).
  if (reason == UnloadedExtensionReason::PROFILE_SHUTDOWN)
    NotifyWebAppProfileWillBeDeleted(extension->id());
}

void BookmarkAppRegistrar::OnShutdown(ExtensionRegistry* registry) {
  NotifyAppRegistrarShutdown();
  extension_observer_.RemoveAll();
}

std::string BookmarkAppRegistrar::GetAppShortName(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkApp(app_id);
  return extension ? extension->short_name() : std::string();
}

std::string BookmarkAppRegistrar::GetAppDescription(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkApp(app_id);
  return extension ? extension->description() : std::string();
}

base::Optional<SkColor> BookmarkAppRegistrar::GetAppThemeColor(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkApp(app_id);
  if (!extension)
    return base::nullopt;

  base::Optional<SkColor> extension_theme_color =
      AppThemeColorInfo::GetThemeColor(extension);
  if (extension_theme_color)
    return SkColorSetA(*extension_theme_color, SK_AlphaOPAQUE);

  return base::nullopt;
}

const GURL& BookmarkAppRegistrar::GetAppLaunchURL(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkApp(app_id);
  return extension ? AppLaunchInfo::GetLaunchWebURL(extension)
                   : GURL::EmptyGURL();
}

base::Optional<GURL> BookmarkAppRegistrar::GetAppScope(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkApp(app_id);
  if (!extension)
    return base::nullopt;

  GURL scope_url = GetScopeURLFromBookmarkApp(GetBookmarkApp(app_id));
  if (scope_url.is_valid())
    return scope_url;

  return base::nullopt;
}

DisplayMode BookmarkAppRegistrar::GetAppDisplayMode(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkApp(app_id);
  if (!extension)
    return DisplayMode::kUndefined;

  return AppDisplayModeInfo::GetDisplayMode(extension);
}

DisplayMode BookmarkAppRegistrar::GetAppUserDisplayMode(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetBookmarkApp(app_id);
  if (!extension)
    return DisplayMode::kStandalone;

  switch (extensions::GetLaunchContainer(
      extensions::ExtensionPrefs::Get(profile()), extension)) {
    case LaunchContainer::kLaunchContainerWindow:
    case LaunchContainer::kLaunchContainerPanelDeprecated:
      return DisplayMode::kStandalone;
    case LaunchContainer::kLaunchContainerTab:
      return DisplayMode::kBrowser;
    case LaunchContainer::kLaunchContainerNone:
      NOTREACHED();
      return DisplayMode::kUndefined;
  }
}

std::vector<WebApplicationIconInfo> BookmarkAppRegistrar::GetAppIconInfos(
    const web_app::AppId& app_id) const {
  std::vector<WebApplicationIconInfo> result;
  const Extension* extension = GetBookmarkApp(app_id);
  if (!extension)
    return result;
  for (const LinkedAppIcons::IconInfo& icon_info :
       LinkedAppIcons::GetLinkedAppIcons(extension).icons) {
    WebApplicationIconInfo web_app_icon_info;
    web_app_icon_info.url = icon_info.url;
    web_app_icon_info.square_size_px = icon_info.size;
    result.push_back(std::move(web_app_icon_info));
  }
  return result;
}

std::vector<web_app::AppId> BookmarkAppRegistrar::GetAppIds() const {
  std::vector<web_app::AppId> app_ids;
  for (scoped_refptr<const Extension> app :
       ExtensionRegistry::Get(profile())->enabled_extensions()) {
    if (app->from_bookmark()) {
      app_ids.push_back(app->id());
    }
  }
  return app_ids;
}

const Extension* BookmarkAppRegistrar::GetBookmarkApp(
    const web_app::AppId& app_id) const {
  const Extension* extension = GetExtension(app_id);
  DCHECK(!extension || extension->from_bookmark());
  return extension;
}

const Extension* BookmarkAppRegistrar::GetExtension(
    const web_app::AppId& app_id) const {
  return ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
      app_id);
}

}  // namespace extensions

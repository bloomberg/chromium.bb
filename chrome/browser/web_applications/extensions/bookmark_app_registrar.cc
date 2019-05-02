// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"

#include <utility>

#include "base/one_shot_event.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

BookmarkAppRegistrar::BookmarkAppRegistrar(Profile* profile)
    : profile_(profile) {
  extension_observer_.Add(ExtensionRegistry::Get(profile));
}

BookmarkAppRegistrar::~BookmarkAppRegistrar() = default;

void BookmarkAppRegistrar::Init(base::OnceClosure callback) {
  ExtensionSystem::Get(profile_)->ready().Post(FROM_HERE, std::move(callback));
}

bool BookmarkAppRegistrar::IsInstalled(const GURL& start_url) const {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const ExtensionSet& extensions = registry->enabled_extensions();

  // Iterate through the extensions and extract the LaunchWebUrl (bookmark apps)
  // or check the web extent (hosted apps).
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (!extension->is_hosted_app())
      continue;

    if (!BookmarkAppIsLocallyInstalled(profile_, extension.get()))
      continue;

    if (extension->web_extent().MatchesURL(start_url) ||
        AppLaunchInfo::GetLaunchWebURL(extension.get()) == start_url) {
      return true;
    }
  }
  return false;
}

bool BookmarkAppRegistrar::IsInstalled(const web_app::AppId& app_id) const {
  return GetExtension(app_id) != nullptr;
}

bool BookmarkAppRegistrar::WasExternalAppUninstalledByUser(
    const web_app::AppId& app_id) const {
  return ExtensionPrefs::Get(profile_)->IsExternalExtensionUninstalled(app_id);
}

bool BookmarkAppRegistrar::HasScopeUrl(const web_app::AppId& app_id) const {
  const auto* extension = GetExtension(app_id);
  DCHECK(extension);

  if (!extension->from_bookmark())
    return false;

  return UrlHandlers::GetUrlHandlers(extension) != nullptr;
}

GURL BookmarkAppRegistrar::GetScopeUrlForApp(
    const web_app::AppId& app_id) const {
  // Returning an empty GURL for a Bookmark App that doesn't have a scope, could
  // lead to incorrecly thinking some URLs are in-scope of the app. To avoid
  // this, CHECK that the Bookmark App has a scope. Callers can use
  // HasScopeUrl() to know if they can call this method.
  CHECK(HasScopeUrl(app_id));

  GURL scope_url = GetScopeURLFromBookmarkApp(GetExtension(app_id));
  CHECK(scope_url.is_valid());
  return scope_url;
}

void BookmarkAppRegistrar::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  for (web_app::AppRegistrarObserver& observer : observers_)
    observer.OnWebAppInstalled(extension->id());
}

void BookmarkAppRegistrar::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  for (web_app::AppRegistrarObserver& observer : observers_)
    observer.OnWebAppUninstalled(extension->id());
}

void BookmarkAppRegistrar::OnShutdown(ExtensionRegistry* registry) {
  for (web_app::AppRegistrarObserver& observer : observers_)
    observer.OnAppRegistrarShutdown();
  extension_observer_.RemoveAll();
}

const Extension* BookmarkAppRegistrar::GetExtension(
    const web_app::AppId& app_id) const {
  return ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(app_id);
}

}  // namespace extensions

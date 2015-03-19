// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_ui_util.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

bool IsBlockedByPolicy(const Extension* app, content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);

  return (app->id() == extensions::kWebStoreAppId ||
      app->id() == extension_misc::kEnterpriseWebStoreAppId) &&
      profile->GetPrefs()->GetBoolean(prefs::kHideWebStoreIcon);
}

bool IsSameOriginOrMoreSecure(const GURL& app_url, const GURL& page_url) {
  return (app_url.scheme() == page_url.scheme() ||
          page_url.scheme() == url::kHttpsScheme) &&
         app_url.host() == page_url.host() &&
         app_url.port() == page_url.port();
}

}  // namespace

namespace ui_util {

bool ShouldDisplayInAppLauncher(const Extension* extension,
                                content::BrowserContext* context) {
  return CanDisplayInAppLauncher(extension, context) &&
         !util::IsEphemeralApp(extension->id(), context);
}

bool CanDisplayInAppLauncher(const Extension* extension,
                             content::BrowserContext* context) {
  return extension->ShouldDisplayInAppLauncher() &&
         !IsBlockedByPolicy(extension, context);
}

bool ShouldDisplayInNewTabPage(const Extension* extension,
                               content::BrowserContext* context) {
  return extension->ShouldDisplayInNewTabPage() &&
      !IsBlockedByPolicy(extension, context) &&
      !util::IsEphemeralApp(extension->id(), context);
}

bool ShouldDisplayInExtensionSettings(const Extension* extension,
                                      content::BrowserContext* context) {
  return extension->ShouldDisplayInExtensionSettings() &&
      !util::IsEphemeralApp(extension->id(), context);
}

bool ShouldNotBeVisible(const Extension* extension,
                        content::BrowserContext* context) {
  return extension->ShouldNotBeVisible() ||
      util::IsEphemeralApp(extension->id(), context);
}

bool ShouldShowLocationBar(const Extension* extension,
                           const content::WebContents* web_contents) {
  // Default to not showing the location bar if either |extension| or
  // |web_contents| are null. |extension| is null for the dev tools.
  if (!extension || !web_contents)
    return false;

  if (!extension->from_bookmark())
    return false;

  GURL launch_url = AppLaunchInfo::GetLaunchWebURL(extension);
  return !(IsSameOriginOrMoreSecure(launch_url,
                                    web_contents->GetVisibleURL()) &&
           IsSameOriginOrMoreSecure(launch_url,
                                    web_contents->GetLastCommittedURL()));
}

}  // namespace ui_util
}  // namespace extensions

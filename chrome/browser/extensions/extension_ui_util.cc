// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_ui_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
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

}  // namespace

namespace ui_util {

bool ShouldDisplayInAppLauncher(const Extension* extension,
                                content::BrowserContext* context) {
  return CanDisplayInAppLauncher(extension, context);
}

bool CanDisplayInAppLauncher(const Extension* extension,
                             content::BrowserContext* context) {
  return extension->ShouldDisplayInAppLauncher() &&
         !IsBlockedByPolicy(extension, context);
}

bool ShouldDisplayInNewTabPage(const Extension* extension,
                               content::BrowserContext* context) {
  return extension->ShouldDisplayInNewTabPage() &&
      !IsBlockedByPolicy(extension, context);
}

bool ShouldDisplayInExtensionSettings(const Extension* extension,
                                      content::BrowserContext* context) {
  return extension->ShouldDisplayInExtensionSettings();
}

bool ShouldNotBeVisible(const Extension* extension,
                        content::BrowserContext* context) {
  return extension->ShouldNotBeVisible();
}

}  // namespace ui_util
}  // namespace extensions

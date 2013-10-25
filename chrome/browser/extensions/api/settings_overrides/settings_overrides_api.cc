// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_overrides/settings_overrides_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

namespace {
base::LazyInstance<ProfileKeyedAPIFactory<SettingsOverridesAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;
}  // namespace

SettingsOverridesAPI::SettingsOverridesAPI(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

SettingsOverridesAPI::~SettingsOverridesAPI() {
}

ProfileKeyedAPIFactory<SettingsOverridesAPI>*
    SettingsOverridesAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void SettingsOverridesAPI::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      const SettingsOverrides* settings =
          SettingsOverrides::Get(extension);
      if (settings && settings->homepage) {
        PreferenceAPI::Get(profile_)->SetExtensionControlledPref(
            extension->id(),
            prefs::kHomePage,
            kExtensionPrefsScopeRegular,
            new base::StringValue(settings->homepage->spec()));
        PreferenceAPI::Get(profile_)->SetExtensionControlledPref(
            extension->id(),
            prefs::kHomePageIsNewTabPage,
            kExtensionPrefsScopeRegular,
            new base::FundamentalValue(false));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      const SettingsOverrides* settings = SettingsOverrides::Get(extension);
      if (settings && settings->homepage) {
        PreferenceAPI::Get(profile_)->RemoveExtensionControlledPref(
            extension->id(),
            prefs::kHomePage,
            kExtensionPrefsScopeRegular);
        PreferenceAPI::Get(profile_)->RemoveExtensionControlledPref(
            extension->id(),
            prefs::kHomePageIsNewTabPage,
            kExtensionPrefsScopeRegular);
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

}  // namespace extensions

// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings_api_helpers.h"

#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"

namespace extensions {

const extensions::SettingsOverrides* FindOverridingExtension(
    content::BrowserContext* browser_context,
    SettingsApiOverrideType type,
    const Extension** extension) {
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(browser_context)->enabled_extensions();

  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end();
       ++it) {
    const extensions::SettingsOverrides* settings =
        extensions::SettingsOverrides::Get(*it);
    if (settings) {
      if (type == BUBBLE_TYPE_HOME_PAGE && !settings->homepage)
        continue;
      if (type == BUBBLE_TYPE_STARTUP_PAGES && settings->startup_pages.empty())
        continue;
      if (type == BUBBLE_TYPE_SEARCH_ENGINE && !settings->search_engine)
        continue;

      std::string key;
      switch (type) {
        case BUBBLE_TYPE_HOME_PAGE:
          key = prefs::kHomePage;
          break;
        case BUBBLE_TYPE_STARTUP_PAGES:
          key = prefs::kRestoreOnStartup;
          break;
        case BUBBLE_TYPE_SEARCH_ENGINE:
          key = prefs::kDefaultSearchProviderEnabled;
          break;
      }

      // Found an extension overriding the current type, check if primary.
      PreferenceAPI* preference_api = PreferenceAPI::Get(browser_context);
      if (preference_api &&  // Expected to be NULL in unit tests.
          !preference_api->DoesExtensionControlPref((*it)->id(), key, NULL))
        continue;  // Not primary.

      // Found the primary extension, return its setting.
      *extension = *it;
      return settings;
    }
  }

  return NULL;
}

const Extension* OverridesHomepage(content::BrowserContext* browser_context,
                                   GURL* home_page_url) {
  const extensions::Extension* extension = NULL;
  const extensions::SettingsOverrides* settings =
      FindOverridingExtension(
          browser_context, BUBBLE_TYPE_HOME_PAGE, &extension);

  if (settings && home_page_url)
    *home_page_url = *settings->homepage;
  return extension;
}

const Extension* OverridesStartupPages(content::BrowserContext* browser_context,
                                       std::vector<GURL>* startup_pages) {
  const extensions::Extension* extension = NULL;
  const extensions::SettingsOverrides* settings =
      FindOverridingExtension(
          browser_context, BUBBLE_TYPE_STARTUP_PAGES, &extension);
  if (settings && startup_pages) {
    startup_pages->clear();
    for (std::vector<GURL>::const_iterator it = settings->startup_pages.begin();
         it != settings->startup_pages.end();
         ++it)
      startup_pages->push_back(GURL(*it));
  }
  return extension;
}

const Extension* OverridesSearchEngine(
    content::BrowserContext* browser_context,
    api::manifest_types::ChromeSettingsOverrides::Search_provider*
        search_provider) {
  const extensions::Extension* extension = NULL;
  const extensions::SettingsOverrides* settings =
      FindOverridingExtension(
          browser_context, BUBBLE_TYPE_SEARCH_ENGINE, &extension);
  if (settings && search_provider)
    search_provider = settings->search_engine.get();
  return extension;
}

}  // namespace extensions

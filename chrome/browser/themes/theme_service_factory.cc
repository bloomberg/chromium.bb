// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_factory.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/common/pref_names.h"

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#endif

// static
ThemeService* ThemeServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ThemeService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
const extensions::Extension* ThemeServiceFactory::GetThemeForProfile(
    Profile* profile) {
  std::string id = GetForProfile(profile)->GetThemeID();
  if (id == ThemeService::kDefaultThemeID)
    return NULL;

  return profile->GetExtensionService()->GetExtensionById(id, false);
}

// static
ThemeServiceFactory* ThemeServiceFactory::GetInstance() {
  return Singleton<ThemeServiceFactory>::get();
}

ThemeServiceFactory::ThemeServiceFactory()
    : ProfileKeyedServiceFactory("ThemeService",
                                 ProfileDependencyManager::GetInstance()) {}

ThemeServiceFactory::~ThemeServiceFactory() {}

ProfileKeyedService* ThemeServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  ThemeService* provider = NULL;
#if defined(TOOLKIT_GTK)
  provider = new GtkThemeService;
#else
  provider = new ThemeService;
#endif
  provider->Init(profile);

  return provider;
}

void ThemeServiceFactory::RegisterUserPrefs(PrefService* prefs) {
#if defined(TOOLKIT_GTK)
  prefs->RegisterBooleanPref(prefs::kUsesSystemTheme,
                             GtkThemeService::DefaultUsesSystemTheme(),
                             PrefService::UNSYNCABLE_PREF);
#endif
  prefs->RegisterFilePathPref(prefs::kCurrentThemePackFilename,
                              FilePath(),
                              PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kCurrentThemeID,
                            ThemeService::kDefaultThemeID,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeImages,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeColors,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeTints,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kCurrentThemeDisplayProperties,
                                PrefService::UNSYNCABLE_PREF);
}

bool ThemeServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

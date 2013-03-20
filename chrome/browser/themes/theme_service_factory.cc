// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_factory.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

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

void ThemeServiceFactory::RegisterUserPrefs(PrefRegistrySyncable* registry) {
#if defined(TOOLKIT_GTK)
  registry->RegisterBooleanPref(prefs::kUsesSystemTheme,
                                GtkThemeService::DefaultUsesSystemTheme(),
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterFilePathPref(prefs::kCurrentThemePackFilename,
                                 base::FilePath(),
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kCurrentThemeID,
                               ThemeService::kDefaultThemeID,
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kCurrentThemeImages,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kCurrentThemeColors,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kCurrentThemeTints,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kCurrentThemeDisplayProperties,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool ThemeServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool ThemeServiceFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

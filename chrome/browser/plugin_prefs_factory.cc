// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_prefs_factory.h"

#include "base/path_service.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"

// static
PluginPrefsFactory* PluginPrefsFactory::GetInstance() {
  return Singleton<PluginPrefsFactory>::get();
}

// static
ProfileKeyedBase* PluginPrefsFactory::CreatePrefsForProfile(
    Profile* profile) {
  return GetInstance()->BuildServiceInstanceFor(profile);
}

PluginPrefs* PluginPrefsFactory::GetPrefsForProfile(Profile* profile) {
  return static_cast<PluginPrefs*>(GetBaseForProfile(profile, true));
}

PluginPrefsFactory::PluginPrefsFactory()
    : RefcountedProfileKeyedServiceFactory(
          "PluginPrefs", ProfileDependencyManager::GetInstance()) {
}

PluginPrefsFactory::~PluginPrefsFactory() {}

RefcountedProfileKeyedService* PluginPrefsFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  PluginPrefs* plugin_prefs = new PluginPrefs();
  plugin_prefs->set_profile(profile->GetOriginalProfile());
  plugin_prefs->SetPrefs(profile->GetPrefs());
  return plugin_prefs;
}

void PluginPrefsFactory::RegisterUserPrefs(PrefService* prefs) {
  FilePath internal_dir;
  PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir);
  prefs->RegisterFilePathPref(prefs::kPluginsLastInternalDirectory,
                              internal_dir,
                              PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsEnabledInternalPDF,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsEnabledNaCl,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsPluginsList,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsDisabledPlugins,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsDisabledPluginsExceptions,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kPluginsEnabledPlugins,
                          PrefService::UNSYNCABLE_PREF);
}

bool PluginPrefsFactory::ServiceRedirectedInIncognito() {
  return true;
}

bool PluginPrefsFactory::ServiceIsNULLWhileTesting() {
  return true;
}

bool PluginPrefsFactory::ServiceIsCreatedWithProfile() {
  return true;
}

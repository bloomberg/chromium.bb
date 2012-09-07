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
scoped_refptr<PluginPrefs> PluginPrefsFactory::GetPrefsForProfile(
    Profile* profile) {
  return static_cast<PluginPrefs*>(
      GetInstance()->GetServiceForProfile(profile, true).get());
}

// static
scoped_refptr<RefcountedProfileKeyedService>
PluginPrefsFactory::CreateForTestingProfile(Profile* profile) {
  return static_cast<PluginPrefs*>(
      GetInstance()->BuildServiceInstanceFor(profile).get());
}

PluginPrefsFactory::PluginPrefsFactory()
    : RefcountedProfileKeyedServiceFactory(
          "PluginPrefs", ProfileDependencyManager::GetInstance()) {
}

PluginPrefsFactory::~PluginPrefsFactory() {}

scoped_refptr<RefcountedProfileKeyedService>
PluginPrefsFactory::BuildServiceInstanceFor(Profile* profile) const {
  scoped_refptr<PluginPrefs> plugin_prefs(new PluginPrefs());
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
  prefs->RegisterBooleanPref(prefs::kPluginsMigratedToPepperFlash,
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

bool PluginPrefsFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool PluginPrefsFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool PluginPrefsFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

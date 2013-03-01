// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs_factory.h"

#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
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

void PluginPrefsFactory::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  base::FilePath internal_dir;
  PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir);
  registry->RegisterFilePathPref(prefs::kPluginsLastInternalDirectory,
                                 internal_dir,
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kPluginsEnabledInternalPDF,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kPluginsEnabledNaCl,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kPluginsMigratedToPepperFlash,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPluginsRemovedOldComponentPepperFlashSettings,
      false,
      PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPluginsPluginsList,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPluginsDisabledPlugins,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPluginsDisabledPluginsExceptions,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPluginsEnabledPlugins,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
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

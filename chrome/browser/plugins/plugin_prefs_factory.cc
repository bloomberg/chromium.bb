// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs_factory.h"

#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"

// static
PluginPrefsFactory* PluginPrefsFactory::GetInstance() {
  return Singleton<PluginPrefsFactory>::get();
}

// static
scoped_refptr<PluginPrefs> PluginPrefsFactory::GetPrefsForProfile(
    Profile* profile) {
  return static_cast<PluginPrefs*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
scoped_refptr<RefcountedBrowserContextKeyedService>
PluginPrefsFactory::CreateForTestingProfile(content::BrowserContext* profile) {
  return static_cast<PluginPrefs*>(
      GetInstance()->BuildServiceInstanceFor(profile).get());
}

PluginPrefsFactory::PluginPrefsFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "PluginPrefs", BrowserContextDependencyManager::GetInstance()) {
}

PluginPrefsFactory::~PluginPrefsFactory() {}

scoped_refptr<RefcountedBrowserContextKeyedService>
PluginPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  scoped_refptr<PluginPrefs> plugin_prefs(new PluginPrefs());
  plugin_prefs->set_profile(profile->GetOriginalProfile());
  plugin_prefs->SetPrefs(profile->GetPrefs());
  return plugin_prefs;
}

void PluginPrefsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  base::FilePath internal_dir;
  PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &internal_dir);
  registry->RegisterFilePathPref(
      prefs::kPluginsLastInternalDirectory,
      internal_dir,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPluginsMigratedToPepperFlash,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPluginsRemovedOldComponentPepperFlashSettings,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPluginsPluginsList,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPluginsDisabledPlugins,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPluginsDisabledPluginsExceptions,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPluginsEnabledPlugins,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

content::BrowserContext* PluginPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool PluginPrefsFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool PluginPrefsFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

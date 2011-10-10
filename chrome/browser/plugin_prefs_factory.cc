// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

namespace {

}  // namespace

PluginPrefsWrapper::PluginPrefsWrapper(scoped_refptr<PluginPrefs> plugin_prefs)
    : plugin_prefs_(plugin_prefs) {
}

PluginPrefsWrapper::~PluginPrefsWrapper() {}

void PluginPrefsWrapper::Shutdown() {
  plugin_prefs_->ShutdownOnUIThread();
}

// static
PluginPrefsFactory* PluginPrefsFactory::GetInstance() {
  return Singleton<PluginPrefsFactory>::get();
}

PluginPrefsWrapper* PluginPrefsFactory::GetWrapperForProfile(
    Profile* profile) {
  return static_cast<PluginPrefsWrapper*>(GetServiceForProfile(profile, true));
}

// static
ProfileKeyedService* PluginPrefsFactory::CreateWrapperForProfile(
    Profile* profile) {
  return GetInstance()->BuildServiceInstanceFor(profile);
}

void PluginPrefsFactory::ForceRegisterPrefsForTest(PrefService* prefs) {
  RegisterUserPrefs(prefs);
}

PluginPrefsFactory::PluginPrefsFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

PluginPrefsFactory::~PluginPrefsFactory() {}

ProfileKeyedService* PluginPrefsFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  scoped_refptr<PluginPrefs> plugin_prefs(new PluginPrefs());
  plugin_prefs->SetPrefs(profile->GetPrefs());
  return new PluginPrefsWrapper(plugin_prefs);
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

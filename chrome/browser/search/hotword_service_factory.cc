// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"

// static
HotwordService* HotwordServiceFactory::GetForProfile(Profile* profile) {
  if (!profile->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled) ||
      (profile->IsOffTheRecord() &&
       !profile->GetPrefs()->GetBoolean(prefs::kHotwordSearchIncognitoEnabled)))
    return NULL;

  return static_cast<HotwordService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
HotwordServiceFactory* HotwordServiceFactory::GetInstance() {
  return Singleton<HotwordServiceFactory>::get();
}

HotwordServiceFactory::HotwordServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "HotwordService",
        BrowserContextDependencyManager::GetInstance()) {
  // No dependencies.
}

HotwordServiceFactory::~HotwordServiceFactory() {
}

void HotwordServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kHotwordSearchEnabled,
                             false,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kHotwordSearchIncognitoEnabled,
                             false,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

content::BrowserContext* HotwordServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

BrowserContextKeyedService* HotwordServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new HotwordService(static_cast<Profile*>(profile));
}

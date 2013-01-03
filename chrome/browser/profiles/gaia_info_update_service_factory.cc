// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service_factory.h"

#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/pref_names.h"

GAIAInfoUpdateServiceFactory::GAIAInfoUpdateServiceFactory()
    : ProfileKeyedServiceFactory("GAIAInfoUpdateService",
                                 ProfileDependencyManager::GetInstance()) {
}

GAIAInfoUpdateServiceFactory::~GAIAInfoUpdateServiceFactory() {}

// static
GAIAInfoUpdateService* GAIAInfoUpdateServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GAIAInfoUpdateService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
GAIAInfoUpdateServiceFactory* GAIAInfoUpdateServiceFactory::GetInstance() {
  return Singleton<GAIAInfoUpdateServiceFactory>::get();
}

ProfileKeyedService* GAIAInfoUpdateServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  if (!GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(profile))
    return NULL;
  return new GAIAInfoUpdateService(profile);
}

void GAIAInfoUpdateServiceFactory::RegisterUserPrefs(
    PrefServiceSyncable* prefs) {
  prefs->RegisterInt64Pref(prefs::kProfileGAIAInfoUpdateTime, 0,
                           PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kProfileGAIAInfoPictureURL, "",
                            PrefServiceSyncable::UNSYNCABLE_PREF);
}

bool GAIAInfoUpdateServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

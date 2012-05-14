// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory.h"

#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/pref_names.h"

TemplateURLService* TemplateURLServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TemplateURLService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

TemplateURLServiceFactory* TemplateURLServiceFactory::GetInstance() {
  return Singleton<TemplateURLServiceFactory>::get();
}

TemplateURLServiceFactory::TemplateURLServiceFactory()
    : ProfileKeyedServiceFactory("TemplateURLServiceFactory",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(GoogleURLTrackerFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
  // TODO(erg): For Shutdown() order, we need to:
  //     DependsOn(HistoryService::GetInstance());
  //     DependsOn(ExtensionService::GetInstance());
}

TemplateURLServiceFactory::~TemplateURLServiceFactory() {}

ProfileKeyedService* TemplateURLServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new TemplateURLService(profile);
}

void TemplateURLServiceFactory::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kSyncedDefaultSearchProviderGUID,
                            std::string(),
                            PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDefaultSearchProviderEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderName,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderID,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderPrepopulateID,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderSuggestURL,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderSearchURL,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderInstantURL,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderKeyword,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderIconURL,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderEncodings,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
}

bool TemplateURLServiceFactory::ServiceRedirectedInIncognito() {
  return true;
}

bool TemplateURLServiceFactory::ServiceIsNULLWhileTesting() {
  return true;
}

void TemplateURLServiceFactory::ProfileShutdown(Profile* profile) {
  // We shutdown AND destroy the TemplateURLService during this pass.
  // TemplateURLService schedules a task on the WebDataService from its
  // destructor. Delete it first to ensure the task gets scheduled before we
  // shut down the database.
  ProfileKeyedServiceFactory::ProfileShutdown(profile);
  ProfileKeyedServiceFactory::ProfileDestroyed(profile);
}

void TemplateURLServiceFactory::ProfileDestroyed(Profile* profile) {
  // Don't double delete.
}

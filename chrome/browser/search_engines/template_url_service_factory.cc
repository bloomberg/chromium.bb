// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory.h"

#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/pref_names.h"

// static
TemplateURLService* TemplateURLServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TemplateURLService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
TemplateURLServiceFactory* TemplateURLServiceFactory::GetInstance() {
  return Singleton<TemplateURLServiceFactory>::get();
}

// static
ProfileKeyedService* TemplateURLServiceFactory::BuildInstanceFor(
    Profile* profile) {
  return new TemplateURLService(profile);
}

TemplateURLServiceFactory::TemplateURLServiceFactory()
    : ProfileKeyedServiceFactory("TemplateURLServiceFactory",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(GoogleURLTrackerFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
}

TemplateURLServiceFactory::~TemplateURLServiceFactory() {}

ProfileKeyedService* TemplateURLServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return BuildInstanceFor(profile);
}

void TemplateURLServiceFactory::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  prefs->RegisterStringPref(prefs::kSyncedDefaultSearchProviderGUID,
                            std::string(),
                            PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDefaultSearchProviderEnabled,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderName,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderID,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderPrepopulateID,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderSuggestURL,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderSearchURL,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderInstantURL,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderKeyword,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderIconURL,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultSearchProviderEncodings,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kDefaultSearchProviderAlternateURLs,
                          PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      std::string(),
      PrefServiceSyncable::UNSYNCABLE_PREF);
}

bool TemplateURLServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool TemplateURLServiceFactory::ServiceIsNULLWhileTesting() const {
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

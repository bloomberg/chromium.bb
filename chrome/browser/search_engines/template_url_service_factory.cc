// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

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

void TemplateURLServiceFactory::RegisterUserPrefs(
    PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSyncedDefaultSearchProviderGUID,
                               std::string(),
                               PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kDefaultSearchProviderEnabled,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderName,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderID,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderPrepopulateID,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderSuggestURL,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderSearchURL,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderInstantURL,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderKeyword,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderIconURL,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kDefaultSearchProviderEncodings,
                               std::string(),
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kDefaultSearchProviderAlternateURLs,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      std::string(),
      PrefRegistrySyncable::UNSYNCABLE_PREF);
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

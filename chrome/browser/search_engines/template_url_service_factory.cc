// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory.h"

#include <string>

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_service.h"

// static
TemplateURLService* TemplateURLServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TemplateURLService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TemplateURLServiceFactory* TemplateURLServiceFactory::GetInstance() {
  return Singleton<TemplateURLServiceFactory>::get();
}

// static
KeyedService* TemplateURLServiceFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  base::Closure dsp_change_callback;
#if defined(ENABLE_RLZ)
  dsp_change_callback =
      base::Bind(base::IgnoreResult(&RLZTracker::RecordProductEvent),
                 rlz_lib::CHROME,
                 RLZTracker::ChromeOmnibox(),
                 rlz_lib::SET_TO_GOOGLE);
#endif
  Profile* profile = static_cast<Profile*>(context);
  return new TemplateURLService(
      profile->GetPrefs(),
      scoped_ptr<SearchTermsData>(new UIThreadSearchTermsData(profile)),
      WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile, Profile::EXPLICIT_ACCESS),
      scoped_ptr<TemplateURLServiceClient>(
          new ChromeTemplateURLServiceClient(profile)),
      GoogleURLTrackerFactory::GetForProfile(profile),
      g_browser_process->rappor_service(),
      dsp_change_callback);
}

TemplateURLServiceFactory::TemplateURLServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "TemplateURLServiceFactory",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(GoogleURLTrackerFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
}

TemplateURLServiceFactory::~TemplateURLServiceFactory() {}

KeyedService* TemplateURLServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}

void TemplateURLServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DefaultSearchManager::RegisterProfilePrefs(registry);
  registry->RegisterStringPref(prefs::kSyncedDefaultSearchProviderGUID,
                               std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDefaultSearchProviderEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderName,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderID,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderPrepopulateID,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderSuggestURL,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderSearchURL,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderInstantURL,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderImageURL,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderNewTabURL,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderSearchURLPostParams,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderSuggestURLPostParams,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderInstantURLPostParams,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderImageURLPostParams,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderKeyword,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderIconURL,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderEncodings,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kDefaultSearchProviderAlternateURLs,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

content::BrowserContext* TemplateURLServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool TemplateURLServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/google/chrome_google_url_tracker_client.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"


// static
GoogleURLTracker* GoogleURLTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<GoogleURLTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GoogleURLTrackerFactory* GoogleURLTrackerFactory::GetInstance() {
  return Singleton<GoogleURLTrackerFactory>::get();
}

GoogleURLTrackerFactory::GoogleURLTrackerFactory()
    : BrowserContextKeyedServiceFactory(
        "GoogleURLTracker",
        BrowserContextDependencyManager::GetInstance()) {
}

GoogleURLTrackerFactory::~GoogleURLTrackerFactory() {
}

KeyedService* GoogleURLTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  scoped_ptr<GoogleURLTrackerClient> client(
      new ChromeGoogleURLTrackerClient(Profile::FromBrowserContext(context)));
  return new GoogleURLTracker(client.Pass(), GoogleURLTracker::NORMAL_MODE);
}

void GoogleURLTrackerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterStringPref(
      prefs::kLastKnownGoogleURL,
      GoogleURLTracker::kDefaultGoogleHomepage,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterStringPref(
      prefs::kLastPromptedGoogleURL,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

content::BrowserContext* GoogleURLTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool GoogleURLTrackerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool GoogleURLTrackerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

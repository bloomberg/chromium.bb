// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_url_tracker_navigation_helper_impl.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/pref_names.h"


// static
GoogleURLTracker* GoogleURLTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<GoogleURLTracker*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
GoogleURLTrackerFactory* GoogleURLTrackerFactory::GetInstance() {
  return Singleton<GoogleURLTrackerFactory>::get();
}

GoogleURLTrackerFactory::GoogleURLTrackerFactory()
    : ProfileKeyedServiceFactory("GoogleURLTracker",
                                 ProfileDependencyManager::GetInstance()) {
}

GoogleURLTrackerFactory::~GoogleURLTrackerFactory() {
}

ProfileKeyedService* GoogleURLTrackerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  scoped_ptr<GoogleURLTrackerNavigationHelper> nav_helper(
      new GoogleURLTrackerNavigationHelperImpl());
  return new GoogleURLTracker(profile, nav_helper.Pass(),
                              GoogleURLTracker::NORMAL_MODE);
}

void GoogleURLTrackerFactory::RegisterUserPrefs(
    PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterStringPref(prefs::kLastKnownGoogleURL,
                                 GoogleURLTracker::kDefaultGoogleHomepage,
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterStringPref(prefs::kLastPromptedGoogleURL,
                                 std::string(),
                                 PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool GoogleURLTrackerFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool GoogleURLTrackerFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool GoogleURLTrackerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/about_signin_internals_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/signin/about_signin_internals.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "google_apis/gaia/gaia_constants.h"

using namespace signin_internals_util;

AboutSigninInternalsFactory::AboutSigninInternalsFactory()
    : ProfileKeyedServiceFactory("AboutSigninInternals",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(TokenServiceFactory::GetInstance());
}

AboutSigninInternalsFactory::~AboutSigninInternalsFactory() {}

// static
AboutSigninInternals* AboutSigninInternalsFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AboutSigninInternals*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
AboutSigninInternalsFactory* AboutSigninInternalsFactory::GetInstance() {
  return Singleton<AboutSigninInternalsFactory>::get();
}

void AboutSigninInternalsFactory::RegisterUserPrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  // SigninManager information for about:signin-internals.
  for (int i = UNTIMED_FIELDS_BEGIN; i < UNTIMED_FIELDS_END; ++i) {
    const std::string pref_path = SigninStatusFieldToString(
        static_cast<UntimedSigninStatusField>(i));
    user_prefs->RegisterStringPref(
        pref_path.c_str(),
        std::string(),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
  for (int i = TIMED_FIELDS_BEGIN; i < TIMED_FIELDS_END; ++i) {
    const std::string value = SigninStatusFieldToString(
        static_cast<TimedSigninStatusField>(i)) + ".value";
    const std::string time = SigninStatusFieldToString(
        static_cast<TimedSigninStatusField>(i)) + ".time";
    user_prefs->RegisterStringPref(
        value.c_str(),
        std::string(),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
    user_prefs->RegisterStringPref(
        time.c_str(),
        std::string(),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
  // TokenService information for about:signin-internals.
  for (size_t i = 0; i < kNumTokenPrefs; i++) {
    const std::string pref = TokenPrefPath(kTokenPrefsArray[i]);
    const std::string value = pref + ".value";
    const std::string status = pref + ".status";
    const std::string time = pref + ".time";
    const std::string time_internal = pref + ".time_internal";
    user_prefs->RegisterStringPref(
        value.c_str(),
        std::string(),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
    user_prefs->RegisterStringPref(
        status.c_str(),
        std::string(),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
    user_prefs->RegisterStringPref(
        time.c_str(),
        std::string(),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
    user_prefs->RegisterInt64Pref(
        time_internal.c_str(),
        0,
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }
}

ProfileKeyedService* AboutSigninInternalsFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  AboutSigninInternals* service = new AboutSigninInternals();
  service->Initialize(static_cast<Profile*>(profile));
  return service;
}

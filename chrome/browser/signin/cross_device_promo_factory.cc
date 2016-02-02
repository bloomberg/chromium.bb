// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_promo_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/cross_device_promo.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "google_apis/gaia/gaia_constants.h"

CrossDevicePromoFactory::CrossDevicePromoFactory()
    : BrowserContextKeyedServiceFactory(
          "CrossDevicePromo",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(GaiaCookieManagerServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

CrossDevicePromoFactory::~CrossDevicePromoFactory() {
}

// static
CrossDevicePromo* CrossDevicePromoFactory::GetForProfile(Profile* profile) {
  return static_cast<CrossDevicePromo*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrossDevicePromoFactory* CrossDevicePromoFactory::GetInstance() {
  return base::Singleton<CrossDevicePromoFactory>::get();
}

void CrossDevicePromoFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kCrossDevicePromoOptedOut, false);
  user_prefs->RegisterBooleanPref(prefs::kCrossDevicePromoShouldBeShown, false);
  user_prefs->RegisterInt64Pref(
      prefs::kCrossDevicePromoObservedSingleAccountCookie,
      base::Time().ToInternalValue());
  user_prefs->RegisterInt64Pref(
      prefs::kCrossDevicePromoNextFetchListDevicesTime,
      base::Time().ToInternalValue());
  user_prefs->RegisterIntegerPref(prefs::kCrossDevicePromoNumDevices, 0);
  user_prefs->RegisterInt64Pref(prefs::kCrossDevicePromoLastDeviceActiveTime,
                                base::Time().ToInternalValue());
}

KeyedService* CrossDevicePromoFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  CrossDevicePromo* service = new CrossDevicePromo(
      SigninManagerFactory::GetForProfile(profile),
      GaiaCookieManagerServiceFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile), profile->GetPrefs());
  return service;
}

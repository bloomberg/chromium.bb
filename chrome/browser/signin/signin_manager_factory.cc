// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_factory.h"

#include "base/prefs/pref_registry_simple.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager.h"

SigninManagerFactory::SigninManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "SigninManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

SigninManagerFactory::~SigninManagerFactory() {
}

#if defined(OS_CHROMEOS)
// static
SigninManagerBase* SigninManagerFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SigninManagerBase*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
SigninManagerBase* SigninManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SigninManagerBase*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

#else
// static
SigninManager* SigninManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninManager* SigninManagerFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}
#endif

// static
SigninManagerFactory* SigninManagerFactory::GetInstance() {
  return Singleton<SigninManagerFactory>::get();
}

void SigninManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kGoogleServicesLastUsername,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kGoogleServicesUserAccountId,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kGoogleServicesUsername,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kGoogleServicesSigninScopedDeviceId,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAutologinEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kReverseAutologinEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kReverseAutologinRejectedEmailList,
                             new base::ListValue,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(
      prefs::kSignedInTime,
      base::Time().ToInternalValue(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  chrome::RegisterLocalAuthPrefs(registry);
}

// static
void SigninManagerFactory::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesUsernamePattern,
                               std::string());
}

void SigninManagerFactory::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SigninManagerFactory::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SigninManagerFactory::NotifyObserversOfSigninManagerCreationForTesting(
    SigninManagerBase* manager) {
  FOR_EACH_OBSERVER(Observer, observer_list_, SigninManagerCreated(manager));
}

KeyedService* SigninManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  SigninManagerBase* service = NULL;
  Profile* profile = static_cast<Profile*>(context);
  SigninClient* client =
      ChromeSigninClientFactory::GetInstance()->GetForProfile(profile);
#if defined(OS_CHROMEOS)
  service = new SigninManagerBase(client);
#else
  service = new SigninManager(
      client, ProfileOAuth2TokenServiceFactory::GetForProfile(profile));
#endif
  service->Initialize(g_browser_process->local_state());
  FOR_EACH_OBSERVER(Observer, observer_list_, SigninManagerCreated(service));
  return service;
}

void SigninManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  SigninManagerBase* manager = static_cast<SigninManagerBase*>(
      GetServiceForBrowserContext(context, false));
  if (manager)
    FOR_EACH_OBSERVER(Observer, observer_list_, SigninManagerShutdown(manager));
  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}

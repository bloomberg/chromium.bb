// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_service_factory.h"

#include "base/prefs/pref_registry.h"
#include "chrome/browser/invalidation/fake_invalidation_service.h"
#include "chrome/browser/invalidation/invalidation_service.h"
#include "chrome/browser/invalidation/invalidation_service_android.h"
#include "chrome/browser/invalidation/invalidator_storage.h"
#include "chrome/browser/invalidation/p2p_invalidation_service.h"
#include "chrome/browser/invalidation/ticl_invalidation_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

#if defined(OS_ANDROID)
#include "chrome/browser/invalidation/invalidation_controller_android.h"
#endif  // defined(OS_ANDROID)

namespace invalidation {

// static
InvalidationService* InvalidationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<InvalidationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InvalidationServiceFactory* InvalidationServiceFactory::GetInstance() {
  return Singleton<InvalidationServiceFactory>::get();
}

InvalidationServiceFactory::InvalidationServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "InvalidationService",
        BrowserContextDependencyManager::GetInstance()),
      build_fake_invalidators_(false) {
#if !defined(OS_ANDROID)
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
#endif
}

InvalidationServiceFactory::~InvalidationServiceFactory() {}

namespace {

BrowserContextKeyedService* BuildP2PInvalidationService(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return new P2PInvalidationService(profile);
}

BrowserContextKeyedService* BuildFakeInvalidationService(
    content::BrowserContext* context) {
  return new FakeInvalidationService();
}

}  // namespace

void InvalidationServiceFactory::SetBuildOnlyFakeInvalidatorsForTest(
    bool test_mode_enabled) {
  build_fake_invalidators_ = test_mode_enabled;
}

P2PInvalidationService*
InvalidationServiceFactory::BuildAndUseP2PInvalidationServiceForTest(
    content::BrowserContext* context) {
  BrowserContextKeyedService* service =
      SetTestingFactoryAndUse(context, BuildP2PInvalidationService);
  return static_cast<P2PInvalidationService*>(service);
}

BrowserContextKeyedService* InvalidationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  if (build_fake_invalidators_) {
    return BuildFakeInvalidationService(context);
  }

#if defined(OS_ANDROID)
  InvalidationServiceAndroid* service = new InvalidationServiceAndroid(
      profile,
      new InvalidationControllerAndroid());
  return service;
#else
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  ProfileOAuth2TokenService* oauth2_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);

  TiclInvalidationService* service = new TiclInvalidationService(
      signin_manager,
      oauth2_token_service,
      profile);
  service->Init();
  return service;
#endif
}

void InvalidationServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  InvalidatorStorage::RegisterProfilePrefs(registry);
}

}  // namespace invalidation

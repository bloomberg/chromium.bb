// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/invalidation/ticl_profile_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/invalidation_state_tracker.h"
#include "components/invalidation/invalidator_storage.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/ticl_invalidation_service.h"
#include "components/invalidation/ticl_settings_provider.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_ANDROID)
#include "chrome/browser/invalidation/invalidation_controller_android.h"
#include "chrome/browser/invalidation/invalidation_service_android.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_identity_provider.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#endif

namespace invalidation {

// static
ProfileInvalidationProvider* ProfileInvalidationProviderFactory::GetForProfile(
    Profile* profile) {
#if defined(OS_CHROMEOS)
  // Using ProfileHelper::GetSigninProfile() here would lead to an infinite loop
  // when this method is called during the creation of the sign-in profile
  // itself. Using ProfileHelper::GetSigninProfileDir() is safe because it does
  // not try to access the sign-in profile.
  if (profile->GetPath() == chromeos::ProfileHelper::GetSigninProfileDir()||
      (chromeos::UserManager::IsInitialized() &&
       chromeos::UserManager::Get()->IsLoggedInAsGuest())) {
    // The Chrome OS login and Chrome OS guest profiles do not have GAIA
    // credentials and do not support invalidation.
    return NULL;
  }
#endif
  return static_cast<ProfileInvalidationProvider*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileInvalidationProviderFactory*
ProfileInvalidationProviderFactory::GetInstance() {
  return Singleton<ProfileInvalidationProviderFactory>::get();
}

ProfileInvalidationProviderFactory::ProfileInvalidationProviderFactory()
    : BrowserContextKeyedServiceFactory(
        "InvalidationService",
        BrowserContextDependencyManager::GetInstance()),
      testing_factory_(NULL) {
#if !defined(OS_ANDROID)
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(LoginUIServiceFactory::GetInstance());
#endif
}

ProfileInvalidationProviderFactory::~ProfileInvalidationProviderFactory() {
}

void ProfileInvalidationProviderFactory::RegisterTestingFactory(
    TestingFactoryFunction testing_factory) {
  testing_factory_ = testing_factory;
}

KeyedService* ProfileInvalidationProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  if (testing_factory_)
    return testing_factory_(context);

#if defined(OS_ANDROID)
  return new ProfileInvalidationProvider(scoped_ptr<InvalidationService>(
      new InvalidationServiceAndroid(profile,
                                     new InvalidationControllerAndroid())));
#else

  scoped_ptr<IdentityProvider> identity_provider;

#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (chromeos::UserManager::IsInitialized() &&
      chromeos::UserManager::Get()->IsLoggedInAsKioskApp() &&
      connector->IsEnterpriseManaged()) {
    identity_provider.reset(new chromeos::DeviceIdentityProvider(
        chromeos::DeviceOAuth2TokenServiceFactory::Get()));
  }
#endif

  if (!identity_provider) {
    identity_provider.reset(new ProfileIdentityProvider(
        SigninManagerFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        LoginUIServiceFactory::GetForProfile(profile)));
  }

  scoped_ptr<TiclInvalidationService> service(new TiclInvalidationService(
      GetUserAgent(),
      identity_provider.Pass(),
      scoped_ptr<TiclSettingsProvider>(
          new TiclProfileSettingsProvider(profile)),
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
      profile->GetRequestContext()));
  service->Init(scoped_ptr<syncer::InvalidationStateTracker>(
      new InvalidatorStorage(profile->GetPrefs())));

  return new ProfileInvalidationProvider(service.PassAs<InvalidationService>());
#endif
}

void ProfileInvalidationProviderFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kInvalidationServiceUseGCMChannel,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  InvalidatorStorage::RegisterProfilePrefs(registry);
}

}  // namespace invalidation

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cryptauth/chrome_cryptauth_service_factory.h"

#include "chrome/browser/chromeos/cryptauth/chrome_cryptauth_service.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/chromeos_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace chromeos {

// static
cryptauth::CryptAuthService*
ChromeCryptAuthServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ChromeCryptAuthService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChromeCryptAuthServiceFactory* ChromeCryptAuthServiceFactory::GetInstance() {
  return base::Singleton<ChromeCryptAuthServiceFactory>::get();
}

ChromeCryptAuthServiceFactory::ChromeCryptAuthServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CryptAuthService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
}

ChromeCryptAuthServiceFactory::~ChromeCryptAuthServiceFactory() {}

KeyedService* ChromeCryptAuthServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(hsuregan): This function, along with this class will be removed.
  return nullptr;
}

void ChromeCryptAuthServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

}  // namespace chromeos

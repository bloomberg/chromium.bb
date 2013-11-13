// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"

namespace policy {

// static
PolicyCertService* PolicyCertServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PolicyCertService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
scoped_ptr<PolicyCertVerifier> PolicyCertServiceFactory::CreateForProfile(
    Profile* profile) {
  DCHECK(!GetInstance()->GetServiceForBrowserContext(profile, false));
  PolicyCertService* service = static_cast<PolicyCertService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
  if (!service)
    return scoped_ptr<PolicyCertVerifier>();
  return service->CreatePolicyCertVerifier();
}

// static
PolicyCertServiceFactory* PolicyCertServiceFactory::GetInstance() {
  return Singleton<PolicyCertServiceFactory>::get();
}

PolicyCertServiceFactory::PolicyCertServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PolicyCertService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(UserNetworkConfigurationUpdaterFactory::GetInstance());
}

PolicyCertServiceFactory::~PolicyCertServiceFactory() {}

BrowserContextKeyedService* PolicyCertServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  UserNetworkConfigurationUpdater* net_conf_updater =
      UserNetworkConfigurationUpdaterFactory::GetForProfile(profile);
  if (!net_conf_updater)
    return NULL;

  // In case of usage of additional trust anchors from an incognito profile, the
  // prefs of the original profile have to be marked.
  return new PolicyCertService(net_conf_updater,
                               profile->GetOriginalProfile()->GetPrefs());
}

content::BrowserContext* PolicyCertServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

void PolicyCertServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kUsedPolicyCertificatesOnce,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool PolicyCertServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace policy

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"

#include "base/memory/singleton.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_manager/user_manager.h"

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

// static
void PolicyCertServiceFactory::SetUsedPolicyCertificates(
    const std::string& user_id) {
  if (UsedPolicyCertificates(user_id))
    return;
  ListPrefUpdate update(g_browser_process->local_state(),
                        prefs::kUsedPolicyCertificates);
  update->AppendString(user_id);
}

// static
void PolicyCertServiceFactory::ClearUsedPolicyCertificates(
    const std::string& user_id) {
  ListPrefUpdate update(g_browser_process->local_state(),
                        prefs::kUsedPolicyCertificates);
  update->Remove(base::StringValue(user_id), NULL);
}

// static
bool PolicyCertServiceFactory::UsedPolicyCertificates(
    const std::string& user_id) {
  base::StringValue value(user_id);
  const base::ListValue* list =
      g_browser_process->local_state()->GetList(prefs::kUsedPolicyCertificates);
  if (!list) {
    NOTREACHED();
    return false;
  }
  return list->Find(value) != list->end();
}

// static
void PolicyCertServiceFactory::RegisterPrefs(PrefRegistrySimple* local_state) {
  local_state->RegisterListPref(prefs::kUsedPolicyCertificates);
}

PolicyCertServiceFactory::PolicyCertServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PolicyCertService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(UserNetworkConfigurationUpdaterFactory::GetInstance());
}

PolicyCertServiceFactory::~PolicyCertServiceFactory() {}

KeyedService* PolicyCertServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  user_manager::User* user = chromeos::ProfileHelper::Get()->GetUserByProfile(
      profile->GetOriginalProfile());
  if (!user)
    return NULL;

  // Backwards compatibility: profiles that used policy-pushed certificates used
  // to have this condition marked in their prefs. This signal has moved to
  // local_state though, to support checking it before the profile is loaded.
  // Check the profile here and update the local_state, if appropriate.
  // TODO(joaodasilva): remove this, eventually.
  PrefService* prefs = profile->GetOriginalProfile()->GetPrefs();
  if (prefs->GetBoolean(prefs::kUsedPolicyCertificatesOnce)) {
    SetUsedPolicyCertificates(user->email());
    prefs->ClearPref(prefs::kUsedPolicyCertificatesOnce);

    if (user_manager->GetLoggedInUsers().size() > 1u) {
      // This login should not have been allowed. After rebooting, local_state
      // will contain the updated list of users that used policy-pushed
      // certificates and this won't happen again.
      // Note that a user becomes logged in before his profile is created.
      LOG(ERROR) << "Shutdown session because a tainted profile was added.";
      g_browser_process->local_state()->CommitPendingWrite();
      prefs->CommitPendingWrite();
      chrome::AttemptUserExit();
    }
  }

  UserNetworkConfigurationUpdater* net_conf_updater =
      UserNetworkConfigurationUpdaterFactory::GetForProfile(profile);
  if (!net_conf_updater)
    return NULL;

  return new PolicyCertService(user->email(), net_conf_updater, user_manager);
}

content::BrowserContext* PolicyCertServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

void PolicyCertServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(joaodasilva): this is used for backwards compatibility.
  // Remove once it's not necessary anymore.
  registry->RegisterBooleanPref(
      prefs::kUsedPolicyCertificatesOnce,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool PolicyCertServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace policy

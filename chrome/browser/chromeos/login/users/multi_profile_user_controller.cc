// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller_delegate.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

namespace {

std::string SanitizeBehaviorValue(const std::string& value) {
  if (value == MultiProfileUserController::kBehaviorUnrestricted ||
      value == MultiProfileUserController::kBehaviorPrimaryOnly ||
      value == MultiProfileUserController::kBehaviorNotAllowed ||
      value == MultiProfileUserController::kBehaviorOwnerPrimaryOnly) {
    return value;
  }

  return std::string(MultiProfileUserController::kBehaviorUnrestricted);
}

}  // namespace

// static
const char MultiProfileUserController::kBehaviorUnrestricted[] = "unrestricted";
const char MultiProfileUserController::kBehaviorPrimaryOnly[] = "primary-only";
const char MultiProfileUserController::kBehaviorNotAllowed[] = "not-allowed";

// Note: this policy value is not a real one an is only returned locally for
// owner users instead of default one kBehaviorUnrestricted.
const char MultiProfileUserController::kBehaviorOwnerPrimaryOnly[] =
    "owner-primary-only";

MultiProfileUserController::MultiProfileUserController(
    MultiProfileUserControllerDelegate* delegate,
    PrefService* local_state)
    : delegate_(delegate),
      local_state_(local_state) {
}

MultiProfileUserController::~MultiProfileUserController() {}

// static
void MultiProfileUserController::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kCachedMultiProfileUserBehavior);
}

// static
void MultiProfileUserController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kMultiProfileUserBehavior,
      kBehaviorUnrestricted,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kMultiProfileNeverShowIntro,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kMultiProfileWarningShowDismissed,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

MultiProfileUserController::UserAllowedInSessionResult
MultiProfileUserController::IsUserAllowedInSession(
    const std::string& user_email) const {
  UserManager* user_manager = UserManager::Get();
  CHECK(user_manager);

  const User* primary_user = user_manager->GetPrimaryUser();
  std::string primary_user_email;
  if (primary_user)
    primary_user_email = primary_user->email();

  // Always allow if there is no primary user or user being checked is the
  // primary user.
  if (primary_user_email.empty() || primary_user_email == user_email)
    return ALLOWED;

  // Owner is not allowed to be secondary user.
  if (user_manager->GetOwnerEmail() == user_email)
    return NOT_ALLOWED_OWNER_AS_SECONDARY;

  // Don't allow profiles potentially tainted by data fetched with policy-pushed
  // certificates to join a multiprofile session.
  if (policy::PolicyCertServiceFactory::UsedPolicyCertificates(user_email))
    return NOT_ALLOWED_POLICY_CERT_TAINTED;

  // Don't allow any secondary profiles if the primary profile is tainted.
  if (policy::PolicyCertServiceFactory::UsedPolicyCertificates(
          primary_user_email)) {
    // Check directly in local_state before checking if the primary user has
    // a PolicyCertService. His profile may have been tainted previously though
    // he didn't get a PolicyCertService created for this session.
    return NOT_ALLOWED_PRIMARY_POLICY_CERT_TAINTED;
  }

  // If the primary profile already has policy certificates installed but hasn't
  // used them yet then it can become tainted at any time during this session;
  // disable secondary profiles in this case too.
  Profile* primary_user_profile =
      primary_user ? ProfileHelper::Get()->GetProfileByUser(primary_user)
                   : NULL;
  policy::PolicyCertService* service =
      primary_user_profile ? policy::PolicyCertServiceFactory::GetForProfile(
                                 primary_user_profile)
                           : NULL;
  if (service && service->has_policy_certificates())
    return NOT_ALLOWED_PRIMARY_POLICY_CERT_TAINTED;

  // No user is allowed if the primary user policy forbids it.
  const std::string primary_user_behavior =
      primary_user_profile->GetPrefs()->GetString(
          prefs::kMultiProfileUserBehavior);
  if (primary_user_behavior == kBehaviorNotAllowed)
    return NOT_ALLOWED_PRIMARY_USER_POLICY_FORBIDS;

  // The user must have 'unrestricted' policy to be a secondary user.
  const std::string behavior = GetCachedValue(user_email);
  return behavior == kBehaviorUnrestricted ? ALLOWED :
                                             NOT_ALLOWED_POLICY_FORBIDS;
}

void MultiProfileUserController::StartObserving(Profile* user_profile) {
  // Profile name could be empty during tests.
  if (user_profile->GetProfileName().empty())
    return;

  scoped_ptr<PrefChangeRegistrar> registrar(new PrefChangeRegistrar);
  registrar->Init(user_profile->GetPrefs());
  registrar->Add(
      prefs::kMultiProfileUserBehavior,
      base::Bind(&MultiProfileUserController::OnUserPrefChanged,
                 base::Unretained(this),
                 user_profile));
  pref_watchers_.push_back(registrar.release());

  OnUserPrefChanged(user_profile);
}

void MultiProfileUserController::RemoveCachedValues(
    const std::string& user_email) {
  DictionaryPrefUpdate update(local_state_,
                              prefs::kCachedMultiProfileUserBehavior);
  update->RemoveWithoutPathExpansion(user_email, NULL);
  policy::PolicyCertServiceFactory::ClearUsedPolicyCertificates(user_email);
}

std::string MultiProfileUserController::GetCachedValue(
    const std::string& user_email) const {
  const base::DictionaryValue* dict =
      local_state_->GetDictionary(prefs::kCachedMultiProfileUserBehavior);
  std::string value;
  if (dict && dict->GetStringWithoutPathExpansion(user_email, &value))
    return SanitizeBehaviorValue(value);

  // Owner is not allowed to be secondary user (see http://crbug.com/385034).
  if (UserManager::Get()->GetOwnerEmail() == user_email)
    return std::string(kBehaviorOwnerPrimaryOnly);

  return std::string(kBehaviorUnrestricted);
}

void MultiProfileUserController::SetCachedValue(
    const std::string& user_email,
    const std::string& behavior) {
  DictionaryPrefUpdate update(local_state_,
                              prefs::kCachedMultiProfileUserBehavior);
  update->SetStringWithoutPathExpansion(user_email,
                                        SanitizeBehaviorValue(behavior));
}

void MultiProfileUserController::CheckSessionUsers() {
  const UserList& users = UserManager::Get()->GetLoggedInUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if (IsUserAllowedInSession((*it)->email()) != ALLOWED) {
      delegate_->OnUserNotAllowed((*it)->email());
      return;
    }
  }
}

void MultiProfileUserController::OnUserPrefChanged(
    Profile* user_profile) {
  std::string user_email = user_profile->GetProfileName();
  CHECK(!user_email.empty());
  user_email = gaia::CanonicalizeEmail(user_email);

  PrefService* prefs = user_profile->GetPrefs();
  if (prefs->FindPreference(prefs::kMultiProfileUserBehavior)
          ->IsDefaultValue()) {
    // Migration code to clear cached default behavior.
    // TODO(xiyuan): Remove this after M35.
    DictionaryPrefUpdate update(local_state_,
                                prefs::kCachedMultiProfileUserBehavior);
    update->RemoveWithoutPathExpansion(user_email, NULL);
  } else {
    const std::string behavior =
        prefs->GetString(prefs::kMultiProfileUserBehavior);
    SetCachedValue(user_email, behavior);
  }

  CheckSessionUsers();
}

}  // namespace chromeos

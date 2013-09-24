// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/multi_profile_user_controller.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/login/multi_profile_user_controller_delegate.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

namespace {

std::string SanitizeBehaviorValue(const std::string& value) {
  if (value == MultiProfileUserController::kBehaviorUnrestricted ||
      value == MultiProfileUserController::kBehaviorPrimaryOnly ||
      value == MultiProfileUserController::kBehaviorNotAllowed) {
    return value;
  }

  return std::string(MultiProfileUserController::kBehaviorUnrestricted);
}

}  // namespace

// static
const char MultiProfileUserController::kBehaviorUnrestricted[] = "unrestricted";
const char MultiProfileUserController::kBehaviorPrimaryOnly[] = "primary-only";
const char MultiProfileUserController::kBehaviorNotAllowed[] = "not-allowed";

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
}

bool MultiProfileUserController::IsUserAllowedInSession(
    const std::string& user_email) const {
  UserManager* user_manager = UserManager::Get();
  CHECK(user_manager);

  std::string primary_user_email;
  if (user_manager->GetPrimaryUser())
    primary_user_email = user_manager->GetPrimaryUser()->email();

  // Always allow if there is no primary user or user being checked is the
  // primary user.
  if (primary_user_email.empty() || primary_user_email == user_email)
    return true;

  // Owner is not allowed to be secondary user.
  if (user_manager->GetOwnerEmail() == user_email)
    return false;

  // No user is allowed if the primary user policy forbids it.
  const std::string primary_user_behavior = GetCachedValue(primary_user_email);
  if (primary_user_behavior == kBehaviorNotAllowed)
    return false;

  // The user must have 'unrestricted' policy to be a secondary user.
  const std::string behavior = GetCachedValue(user_email);
  return behavior == kBehaviorUnrestricted;
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

void MultiProfileUserController::RemoveCachedValue(
    const std::string& user_email) {
  DictionaryPrefUpdate update(local_state_,
                              prefs::kCachedMultiProfileUserBehavior);
  update->RemoveWithoutPathExpansion(user_email, NULL);
}

std::string MultiProfileUserController::GetCachedValue(
    const std::string& user_email) const {
  const DictionaryValue* dict =
      local_state_->GetDictionary(prefs::kCachedMultiProfileUserBehavior);
  std::string value;
  if (dict && dict->GetStringWithoutPathExpansion(user_email, &value))
    return SanitizeBehaviorValue(value);

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
    if (!IsUserAllowedInSession((*it)->email())) {
      delegate_->OnUserNotAllowed();
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
  const std::string behavior =
      prefs->GetString(prefs::kMultiProfileUserBehavior);
  SetCachedValue(user_email, behavior);

  CheckSessionUsers();
}

}  // namespace chromeos

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync_username_test_base.h"

#include "base/strings/utf_string_conversions.h"
#include "components/signin/core/browser/signin_pref_names.h"

using autofill::PasswordForm;

namespace password_manager {

SyncUsernameTestBase::LocalFakeSyncService::LocalFakeSyncService()
    : syncing_passwords_(true) {}

SyncUsernameTestBase::LocalFakeSyncService::~LocalFakeSyncService() {}

syncer::ModelTypeSet
SyncUsernameTestBase::LocalFakeSyncService::GetPreferredDataTypes() const {
  if (syncing_passwords_)
    return syncer::ModelTypeSet(syncer::PASSWORDS);
  return syncer::ModelTypeSet();
}

SyncUsernameTestBase::SyncUsernameTestBase()
    : signin_client_(&prefs_),
      token_service_(&prefs_),
#if defined(OS_CHROMEOS)
      signin_manager_(&signin_client_,
                      &account_tracker_,
                      nullptr /* signin_error_controller */),
#else
      signin_manager_(&signin_client_,
                      &token_service_,
                      &account_tracker_,
                      nullptr, /* cookie_manager_service */
                      nullptr, /* signin_error_controller */
                      signin::AccountConsistencyMethod::kDisabled),
#endif
      gaia_cookie_manager_service_(&token_service_,
                                   "sync_username_test_base",
                                   &signin_client_),
      identity_test_env_(&account_tracker_,
                         &token_service_,
                         &signin_manager_,
                         &gaia_cookie_manager_service_) {
  SigninManagerBase::RegisterProfilePrefs(prefs_.registry());
  AccountTrackerService::RegisterPrefs(prefs_.registry());
#if !defined(OS_CHROMEOS)
  ProfileOAuth2TokenService::RegisterProfilePrefs(prefs_.registry());
#endif
  account_tracker_.Initialize(&prefs_, base::FilePath());
}

SyncUsernameTestBase::~SyncUsernameTestBase() {}

void SyncUsernameTestBase::FakeSigninAs(const std::string& email) {
  // This method is called in a roll by some tests. Differently than
  // SigninManager, IdentityTestEnvironment does not allow logging in
  // without a previously log-out.
  // So make sure tests only log in once and that the email is the same
  // in case of FakeSigninAs calls roll.
  identity::IdentityManager* identity_manager =
      identity_test_env_.identity_manager();
  if (identity_manager->HasPrimaryAccount()) {
    DCHECK_EQ(identity_manager->GetPrimaryAccountInfo().email, email);
  } else {
    identity_test_env_.MakePrimaryAccountAvailable(email);
  }
}

// static
PasswordForm SyncUsernameTestBase::SimpleGaiaForm(const char* username) {
  PasswordForm form;
  form.signon_realm = "https://accounts.google.com";
  form.username_value = base::ASCIIToUTF16(username);
  return form;
}

// static
PasswordForm SyncUsernameTestBase::SimpleNonGaiaForm(const char* username) {
  PasswordForm form;
  form.signon_realm = "https://site.com";
  form.username_value = base::ASCIIToUTF16(username);
  return form;
}

// static
PasswordForm SyncUsernameTestBase::SimpleNonGaiaForm(const char* username,
                                                     const char* origin) {
  PasswordForm form;
  form.signon_realm = "https://site.com";
  form.username_value = base::ASCIIToUTF16(username);
  form.origin = GURL(origin);
  return form;
}

void SyncUsernameTestBase::SetSyncingPasswords(bool syncing_passwords) {
  sync_service_.set_syncing_passwords(syncing_passwords);
}

}  // namespace password_manager

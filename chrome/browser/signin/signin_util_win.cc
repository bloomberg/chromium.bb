// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util_win.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"

namespace signin_util {

namespace {

std::unique_ptr<DiceTurnSyncOnHelper::Delegate>*
GetDiceTurnSyncOnHelperDelegateForTestingStorage() {
  static std::unique_ptr<DiceTurnSyncOnHelper::Delegate> delegate;
  return &delegate;
}

std::string DecryptRefreshToken(const std::string& cipher_text) {
  std::string refresh_token;
  if (!OSCrypt::DecryptString(cipher_text, &refresh_token))
    return std::string();

  return refresh_token;
}

// Finish the process of signing in with the credential provider.  This is
// either called directly from SigninWithCredentialProvider() is a browser
// window for the profile is already available or is delayed until a browser
// can first be opened.
void FinishSigninWithCredentialProvider(const std::string& account_id,
                                        Browser* browser,
                                        Profile* profile,
                                        Profile::CreateStatus status) {
  // DiceTurnSyncOnHelper deletes itself once done.
  if (GetDiceTurnSyncOnHelperDelegateForTestingStorage()->get()) {
    new DiceTurnSyncOnHelper(
        profile, signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON,
        signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
        signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT, account_id,
        DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
        std::move(*GetDiceTurnSyncOnHelperDelegateForTestingStorage()));
  } else {
    if (!browser)
      browser = chrome::FindLastActiveWithProfile(profile);

    new DiceTurnSyncOnHelper(
        profile, browser,
        signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON,
        signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
        signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT, account_id,
        DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  }

  // Mark this profile as having been signed in with the credential provider.
  profile->GetPrefs()->SetBoolean(prefs::kSignedInWithCredentialProvider, true);

  // TODO(rogerta): delete the refresh_token regkey.
}

// Start the process of signing in with the credential provider given that
// all the required information is available.  The process depends on having
// a browser window for the profile.  If a browser window exists the profile
// be signed in and sync will be starting up.  If not, the profile will be
// still be signed in but sync will be started once the browser window is
// ready.
void SigninWithCredentialProvider(Profile* profile,
                                  const base::string16& gaia_id,
                                  const base::string16& email,
                                  const std::string& refresh_token) {
  // For debugging purposes, record that the credentials for this profile
  // came from a credential provider.
  AboutSigninInternals* signin_internals =
      AboutSigninInternalsFactory::GetInstance()->GetForProfile(profile);
  signin_internals->OnAuthenticationResultReceived("Credential Provider");

  // First seed the account tracker before adding the account to the token
  // service.
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile);
  std::string account_id = account_tracker->SeedAccountInfo(
      base::UTF16ToUTF8(gaia_id), base::UTF16ToUTF8(email));

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  token_service->UpdateCredentials(
      account_id, refresh_token,
      signin_metrics::SourceForRefreshTokenOperation::
          kMachineLogon_CredentialProvider);

  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  if (browser) {
    FinishSigninWithCredentialProvider(account_id, browser, profile,
                                       Profile::CREATE_STATUS_CREATED);
  } else {
    // If no active browser exists yet, this profile is in the process of
    // being created.  Wait for the browser to be created before finishing the
    // sign in.  This object deletes itself when done.
    new profiles::BrowserAddedForProfileObserver(
        profile, base::BindRepeating(&FinishSigninWithCredentialProvider,
                                     account_id, nullptr));
  }
}

}  // namespace

void SetDiceTurnSyncOnHelperDelegateForTesting(
    std::unique_ptr<DiceTurnSyncOnHelper::Delegate> delegate) {
  GetDiceTurnSyncOnHelperDelegateForTestingStorage()->swap(delegate);
}

void SigninWithCredentialProviderIfPossible(Profile* profile) {
  // Check to see if auto signin information is available.  Only applies if:
  //
  //  - This is first run.
  //  - Opening the initial profile.
  //  - Not already signed in.
  if (!(first_run::IsChromeFirstRun() &&
        g_browser_process->profile_manager()->GetInitialProfileDir() ==
            profile->GetPath().BaseName() &&
        !IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount())) {
    return;
  }

  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, credential_provider::kRegHkcuAccountsPath,
               KEY_READ) != ERROR_SUCCESS) {
    return;
  }

  base::win::RegistryKeyIterator it(key.Handle(), L"");
  if (!it.Valid() || it.SubkeyCount() == 0)
    return;

  base::win::RegKey key_account(key.Handle(), it.Name(), KEY_READ | KEY_WRITE);
  if (!key_account.Valid())
    return;

  base::string16 gaia_id = it.Name();
  base::string16 email;
  if (key_account.ReadValue(
          base::UTF8ToUTF16(credential_provider::kKeyEmail).c_str(), &email) !=
      ERROR_SUCCESS) {
    return;
  }

  // Read the encrypted refresh token.  The data is stored in binary format.
  // No matter what happens, delete the registry entry.
  std::string encrypted_refresh_token;
  DWORD size = 0;
  DWORD type;
  if (key_account.ReadValue(
          base::UTF8ToUTF16(credential_provider::kKeyRefreshToken).c_str(),
          nullptr, &size, &type) != ERROR_SUCCESS) {
    return;
  }

  encrypted_refresh_token.resize(size);
  key_account.ReadValue(
      base::UTF8ToUTF16(credential_provider::kKeyRefreshToken).c_str(),
      const_cast<char*>(encrypted_refresh_token.c_str()), &size, &type);
  if (!gaia_id.empty() && !email.empty() && type == REG_BINARY &&
      !encrypted_refresh_token.empty()) {
    std::string refresh_token = DecryptRefreshToken(encrypted_refresh_token);
    if (!refresh_token.empty())
      SigninWithCredentialProvider(profile, gaia_id, email, refresh_token);
  }

  key_account.DeleteValue(
      base::UTF8ToUTF16(credential_provider::kKeyRefreshToken).c_str());
}

}  // namespace signin_util

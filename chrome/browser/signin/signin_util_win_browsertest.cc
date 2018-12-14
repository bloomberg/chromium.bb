// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_reg_util_win.h"
#include "build/build_config.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_util_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_pref_names.h"

namespace {

class TestDiceTurnSyncOnHelperDelegate : public DiceTurnSyncOnHelper::Delegate {
  ~TestDiceTurnSyncOnHelperDelegate() override {}

  // DiceTurnSyncOnHelper::Delegate:
  void ShowLoginError(const std::string& email,
                      const std::string& error_message) override {}
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override {
    std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
  }
  void ShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override {
    std::move(callback).Run(DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
  }
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override {
    std::move(callback).Run(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  }
  void ShowSyncSettings() override {}
  void ShowSigninPageInNewProfile(Profile* new_profile,
                                  const std::string& username) override {}
};

struct SigninUtilWinBrowserTestParams {
  SigninUtilWinBrowserTestParams(bool is_first_run,
                                 const base::string16& gaia_id,
                                 const base::string16& email,
                                 const std::string& refresh_token,
                                 bool expect_is_started)
      : is_first_run(is_first_run),
        gaia_id(gaia_id),
        email(email),
        refresh_token(refresh_token),
        expect_is_started(expect_is_started) {}

  bool is_first_run = false;
  base::string16 gaia_id;
  base::string16 email;
  std::string refresh_token;
  bool expect_is_started = false;
};

void AssertSigninStarted(bool expect_is_started, Profile* profile) {
  ASSERT_EQ(expect_is_started, profile->GetPrefs()->GetBoolean(
                                   prefs::kSignedInWithCredentialProvider));
}

}  // namespace

class SigninUtilWinBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<SigninUtilWinBrowserTestParams> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(GetParam().is_first_run
                                   ? switches::kForceFirstRun
                                   : switches::kNoFirstRun);
  }

  bool SetUpUserDataDirectory() override {
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);

    base::win::RegKey key;

    if (!GetParam().gaia_id.empty()) {
      EXPECT_EQ(
          ERROR_SUCCESS,
          key.Create(HKEY_CURRENT_USER,
                     credential_provider::kRegHkcuAccountsPath, KEY_WRITE));
      EXPECT_EQ(ERROR_SUCCESS,
                key.CreateKey(GetParam().gaia_id.c_str(), KEY_WRITE));
    }

    if (!GetParam().email.empty()) {
      EXPECT_TRUE(key.Valid());
      EXPECT_EQ(ERROR_SUCCESS,
                key.WriteValue(
                    base::ASCIIToUTF16(credential_provider::kKeyEmail).c_str(),
                    GetParam().email.c_str()));
    }

    if (!GetParam().refresh_token.empty()) {
      EXPECT_TRUE(key.Valid());
      std::string ciphertext;
      EXPECT_TRUE(
          OSCrypt::EncryptString(GetParam().refresh_token, &ciphertext));
      EXPECT_EQ(
          ERROR_SUCCESS,
          key.WriteValue(
              base::ASCIIToUTF16(credential_provider::kKeyRefreshToken).c_str(),
              ciphertext.c_str(), ciphertext.length(), REG_BINARY));
    }

    if (GetParam().expect_is_started) {
      signin_util::SetDiceTurnSyncOnHelperDelegateForTesting(
          std::unique_ptr<DiceTurnSyncOnHelper::Delegate>(
              new TestDiceTurnSyncOnHelperDelegate()));
    }

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
};

IN_PROC_BROWSER_TEST_P(SigninUtilWinBrowserTest, Run) {
  ASSERT_EQ(GetParam().is_first_run, first_run::IsChromeFirstRun());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  Profile* profile =
      profile_manager->GetLastUsedProfile(profile_manager->user_data_dir());
  ASSERT_EQ(profile_manager->GetInitialProfileDir(),
            profile->GetPath().BaseName());

  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  ASSERT_NE(nullptr, browser);

  AssertSigninStarted(GetParam().expect_is_started, profile);

  // If a refresh token was specified and a sign in attempt was expected, make
  // sure the refresh token was removed from the registry.
  if (!GetParam().refresh_token.empty() && GetParam().expect_is_started) {
    base::win::RegKey key;
    EXPECT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER,
                       credential_provider::kRegHkcuAccountsPath, KEY_READ));
    EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(GetParam().gaia_id.c_str(), KEY_READ));
    EXPECT_FALSE(key.HasValue(
        base::ASCIIToUTF16(credential_provider::kKeyRefreshToken).c_str()));
  }
}

INSTANTIATE_TEST_CASE_P(SigninUtilWinBrowserTest1,
                        SigninUtilWinBrowserTest,
                        testing::Values(SigninUtilWinBrowserTestParams(
                            /*is_first_run=*/false,
                            /*gaia_id=*/base::string16(),
                            /*email=*/base::string16(),
                            /*refresh_token=*/std::string(),
                            /*expect_is_started=*/false)));

INSTANTIATE_TEST_CASE_P(SigninUtilWinBrowserTest2,
                        SigninUtilWinBrowserTest,
                        testing::Values(SigninUtilWinBrowserTestParams(
                            /*is_first_run=*/true,
                            /*gaia_id=*/base::string16(),
                            /*email=*/base::string16(),
                            /*refresh_token=*/std::string(),
                            /*expect_is_started=*/false)));

INSTANTIATE_TEST_CASE_P(SigninUtilWinBrowserTest3,
                        SigninUtilWinBrowserTest,
                        testing::Values(SigninUtilWinBrowserTestParams(
                            /*is_first_run=*/true,
                            /*gaia_id=*/L"gaia-123456",
                            /*email=*/base::string16(),
                            /*refresh_token=*/std::string(),
                            /*expect_is_started=*/false)));

INSTANTIATE_TEST_CASE_P(SigninUtilWinBrowserTest4,
                        SigninUtilWinBrowserTest,
                        testing::Values(SigninUtilWinBrowserTestParams(
                            /*is_first_run=*/true,
                            /*gaia_id=*/L"gaia-123456",
                            /*email=*/L"foo@gmail.com",
                            /*refresh_token=*/std::string(),
                            /*expect_is_started=*/false)));

INSTANTIATE_TEST_CASE_P(SigninUtilWinBrowserTest5,
                        SigninUtilWinBrowserTest,
                        testing::Values(SigninUtilWinBrowserTestParams(
                            /*is_first_run=*/true,
                            /*gaia_id=*/L"gaia-123456",
                            /*email=*/L"foo@gmail.com",
                            /*refresh_token=*/"lst-123456",
                            /*expect_is_started=*/true)));

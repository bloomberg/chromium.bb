// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

namespace {

HRESULT CreateGaiaCredentialWithProvider(
    IGaiaCredentialProvider* provider,
    IGaiaCredential** gaia_credential,
    ICredentialProviderCredential** credential) {
  return CreateBaseInheritedCredentialWithProvider<CGaiaCredential>(
      provider, gaia_credential, credential);
}

}  // namespace

class GcpGaiaCredentialTest : public GlsRunnerTestBase {
 protected:
  GcpGaiaCredentialTest();

  FakeGaiaCredentialProvider* provider() { return &provider_; }
  BSTR signin_result() { return signin_result_; }

  CComBSTR MakeSigninResult(const std::string& password);

 private:
  FakeGaiaCredentialProvider provider_;
  CComBSTR signin_result_;
};

GcpGaiaCredentialTest::GcpGaiaCredentialTest() {
  signin_result_ = MakeSigninResult("password");
}

CComBSTR GcpGaiaCredentialTest::MakeSigninResult(const std::string& password) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;
  test_data_storage.SetSigninPassword(password);

  std::string signin_result_utf8;
  EXPECT_TRUE(base::JSONWriter::Write(test_data_storage.expected_full_result(),
                                      &signin_result_utf8));
  return CComBSTR(A2OLE(signin_result_utf8.c_str()));
}

TEST_F(GcpGaiaCredentialTest, OnUserAuthenticated) {
  USES_CONVERSION;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK,
            CreateGaiaCredentialWithProvider(provider(), &gaia_cred, &cred));

  CComBSTR error;
  ASSERT_EQ(S_OK, gaia_cred->OnUserAuthenticated(signin_result(), &error));
  EXPECT_TRUE(provider()->credentials_changed_fired());
}

TEST_F(GcpGaiaCredentialTest, OnUserAuthenticated_SamePassword) {
  USES_CONVERSION;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK,
            CreateGaiaCredentialWithProvider(provider(), &gaia_cred, &cred));

  CComBSTR error;
  ASSERT_EQ(S_OK, gaia_cred->OnUserAuthenticated(signin_result(), &error));

  CComBSTR first_sid = provider()->sid();

  // Report to register the user.
  wchar_t* report_status_text = nullptr;
  CREDENTIAL_PROVIDER_STATUS_ICON report_icon;
  EXPECT_EQ(S_OK, cred->ReportResult(0, 0, &report_status_text, &report_icon));

  // Finishing with the same username+password should succeed.
  CComBSTR error2;
  ASSERT_EQ(S_OK, gaia_cred->OnUserAuthenticated(signin_result(), &error2));
  EXPECT_TRUE(provider()->credentials_changed_fired());
  EXPECT_EQ(first_sid, provider()->sid());
}

TEST_F(GcpGaiaCredentialTest, OnUserAuthenticated_DiffPassword) {
  USES_CONVERSION;

  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR sid;
  ASSERT_EQ(
      S_OK,
      fake_os_user_manager()->CreateTestOSUser(
          L"foo_bar",
          base::UTF8ToUTF16(test_data_storage.GetSuccessPassword()).c_str(),
          base::UTF8ToUTF16(test_data_storage.GetSuccessFullName()).c_str(),
          L"comment",
          base::UTF8ToUTF16(test_data_storage.GetSuccessId()).c_str(),
          base::UTF8ToUTF16(test_data_storage.GetSuccessEmail()).c_str(),
          &sid));
  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
                      nullptr, IID_IGaiaCredential, (void**)&cred));
  ASSERT_EQ(S_OK, cred->Initialize(provider()));

  CComBSTR error;
  ASSERT_EQ(S_OK, cred->OnUserAuthenticated(signin_result(), &error));
  EXPECT_TRUE(provider()->credentials_changed_fired());

  provider()->ResetCredentialsChangedFired();

  CComBSTR new_signin_result = MakeSigninResult("password2");

  // Finishing with the same username but different password should mark
  // the password as stale and not fire the credentials changed event.
  EXPECT_EQ(S_FALSE, cred->OnUserAuthenticated(new_signin_result, &error));
  EXPECT_FALSE(provider()->credentials_changed_fired());
}

class GcpGaiaCredentialGlsRunnerTest : public GlsRunnerTestBase {};

TEST_F(GcpGaiaCredentialGlsRunnerTest,
       AssociateToExistingAssociatedUser_LongUsername) {
  USES_CONVERSION;

  // Create a fake user that has the same username but a different gaia id
  // as the test gaia id.
  CComBSTR sid;
  base::string16 base_username(L"foo1234567890abcdefg");
  base::string16 base_gaia_id(L"other-gaia-id");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      base_username.c_str(), L"password", L"name", L"comment",
                      base_gaia_id, base::string16(), &sid));

  ASSERT_EQ(2u, fake_os_user_manager()->GetUserCount());
  FakeGaiaCredentialProvider provider;

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK,
            CreateGaiaCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(base::UTF16ToUTF8(base_username) +
                                           "@gmail.com"));
  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  // New username should be truncated at the end and have the last character
  // replaced with a new index
  EXPECT_STREQ((base_username.substr(0, base_username.size() - 1) +
                base::NumberToString16(kInitialDuplicateUsernameIndex))
                   .c_str(),
               provider.username());
  EXPECT_NE(0u, provider.password().Length());
  EXPECT_NE(0u, provider.sid().Length());
  EXPECT_STREQ(test->GetErrorText(), nullptr);
  EXPECT_EQ(TRUE, provider.credentials_changed_fired());
  // New user should be created.
  EXPECT_EQ(3u, fake_os_user_manager()->GetUserCount());

  EXPECT_EQ(S_OK, gaia_cred->Terminate());
}

// This test checks the expected success / failure of user creation when
// GCPW wants to associate a gaia id to a user that may already be associated
// to another gaia id.
// Parameters:
// int: Number of existing users to create before trying to associate the
// new user.
// bool: Whether the final user creation is expected to succeed. For
// bool: whether the created users are associated to a gaia id.
// kMaxAttempts = 10, 0 to 8 users can be created and still have the
// test succeed. If a 9th user is create the test will fail.
class GcpAssociatedUserRunnableGaiaCredentialTest
    : public GcpGaiaCredentialGlsRunnerTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

TEST_P(GcpAssociatedUserRunnableGaiaCredentialTest,
       AssociateToExistingAssociatedUser) {
  USES_CONVERSION;
  int last_user_index = std::get<0>(GetParam());
  bool should_succeed = std::get<1>(GetParam());
  bool should_associate = std::get<2>(GetParam());

  // Create many fake users that has the same username but a different gaia id
  // as the test gaia id.
  CComBSTR sid;
  base::string16 base_username(L"foo");
  base::string16 base_gaia_id(L"other-gaia-id");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      base_username.c_str(), L"password", L"name", L"comment",
                      should_associate ? base_gaia_id : base::string16(),
                      base::string16(), &sid));
  ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(sid), A2CW(kKeyId), base_gaia_id));

  for (int i = 0; i < last_user_index; ++i) {
    base::string16 user_suffix =
        base::NumberToString16(i + kInitialDuplicateUsernameIndex);
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        (base_username + user_suffix).c_str(), L"password",
                        L"name", L"comment",
                        should_associate ? base_gaia_id + user_suffix
                                         : base::string16(),
                        base::string16(), &sid));
  }

  ASSERT_EQ(static_cast<size_t>(1 + last_user_index + 1),
            fake_os_user_manager()->GetUserCount());
  FakeGaiaCredentialProvider provider;

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK,
            CreateGaiaCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  if (should_succeed) {
    EXPECT_STREQ(
        (base_username + base::NumberToString16(last_user_index +
                                                kInitialDuplicateUsernameIndex))
            .c_str(),
        provider.username());
    EXPECT_NE(0u, provider.password().Length());
    EXPECT_NE(0u, provider.sid().Length());
    EXPECT_STREQ(test->GetErrorText(), nullptr);
    EXPECT_EQ(TRUE, provider.credentials_changed_fired());
    // New user should be created.
    EXPECT_EQ(static_cast<size_t>(last_user_index + 2 + 1),
              fake_os_user_manager()->GetUserCount());
  } else {
    EXPECT_EQ(0u, provider.username().Length());
    EXPECT_EQ(0u, provider.password().Length());
    EXPECT_EQ(0u, provider.sid().Length());
    EXPECT_STREQ(test->GetErrorText(),
                 GetStringResource(IDS_INTERNAL_ERROR_BASE).c_str());
    EXPECT_EQ(FALSE, provider.credentials_changed_fired());
    // No new user should be created.
    EXPECT_EQ(static_cast<size_t>(last_user_index + 1 + 1),
              fake_os_user_manager()->GetUserCount());
  }
  // Expect a different user name with the suffix added.
  EXPECT_EQ(S_OK, gaia_cred->Terminate());
}

// For a max retry of 10, it is possible to create users 'username',
// 'username0' ... 'username8' before failing. At 'username9' the test should
// fail.

INSTANTIATE_TEST_SUITE_P(
    AvailableUsername,
    GcpAssociatedUserRunnableGaiaCredentialTest,
    ::testing::Combine(::testing::Range(0, kMaxUsernameAttempts - 2),
                       ::testing::Values(true),
                       ::testing::Values(true, false)));

INSTANTIATE_TEST_SUITE_P(
    UnavailableUsername,
    GcpAssociatedUserRunnableGaiaCredentialTest,
    ::testing::Combine(::testing::Values(kMaxUsernameAttempts - 1),
                       ::testing::Values(false),
                       ::testing::Values(true, false)));
}  // namespace testing

}  // namespace credential_provider

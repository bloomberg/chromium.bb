// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

#include "base/json/json_writer.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/reauth_credential.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/fake_gls_run_helper.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace {

HRESULT CreateReauthCredentialWithProvider(
    IGaiaCredentialProvider* provider,
    IGaiaCredential** gaia_credential,
    ICredentialProviderCredential** credential) {
  return testing::CreateInheritedCredentialWithProvider<CReauthCredential,
                                                        IReauthCredential>(
      provider, gaia_credential, credential);
}

}  // namespace

class GcpReauthCredentialTest : public ::testing::Test {
 private:
  void SetUp() override;

  registry_util::RegistryOverrideManager registry_override_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

void GcpReauthCredentialTest::SetUp() {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
}

TEST_F(GcpReauthCredentialTest, SetUserInfo) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CReauthCredential>>::CreateInstance(
                      nullptr, IID_IReauthCredential, (void**)&reauth));
  ASSERT_TRUE(!!reauth);

  const CComBSTR kSid(W2COLE(L"sid"));
  ASSERT_EQ(S_OK,
            reauth->SetUserInfo(
                kSid, CComBSTR(W2COLE(L"username")),
                CComBSTR(A2COLE(test_data_storage.GetSuccessEmail().c_str()))));

  CComPtr<ICredentialProviderCredential2> cpc2;
  ASSERT_EQ(S_OK, reauth->QueryInterface(IID_ICredentialProviderCredential2,
                                         reinterpret_cast<void**>(&cpc2)));
  wchar_t* sid;
  CComBSTR username;
  ASSERT_EQ(S_OK, cpc2->GetUserSid(&sid));
  ASSERT_EQ(kSid, CComBSTR(W2COLE(sid)));
  ::CoTaskMemFree(sid);
}

TEST_F(GcpReauthCredentialTest, UsernameMismatch) {
  USES_CONVERSION;

  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  // Create a valid result with the right user name.
  std::string signin_result_utf8;
  EXPECT_TRUE(base::JSONWriter::Write(test_data_storage.expected_full_result(),
                                      &signin_result_utf8));
  CComBSTR valid_signin_result = A2COLE(signin_result_utf8.c_str());

  // Create an invalid result with the wrong user name.
  base::Value copy_full_result =
      test_data_storage.expected_full_result().Clone();
  copy_full_result.SetKey(kKeyEmail, base::Value("foo_bar2@gmail.com"));
  EXPECT_TRUE(base::JSONWriter::Write(copy_full_result, &signin_result_utf8));
  CComBSTR invalid_signin_result = A2COLE(signin_result_utf8.c_str());

  // Create a fake credential provider.  This object must outlive the reauth
  // credential so it should be declared first.
  FakeGaiaCredentialProvider provider;

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth.
  OSUserManager* manager = OSUserManager::Get();
  CComBSTR sid;
  DWORD error;
  ASSERT_EQ(S_OK, manager->AddUser(username, password, full_name, L"comment",
                                   true, &sid, &error));

  // Initialize a reauth credential.
  CComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CReauthCredential>>::CreateInstance(
                      nullptr, IID_IReauthCredential, (void**)&reauth));

  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, reauth->QueryInterface(IID_IGaiaCredential,
                                         reinterpret_cast<void**>(&cred)));
  ASSERT_EQ(S_OK, cred->Initialize(&provider));

  ASSERT_EQ(S_OK, reauth->SetUserInfo(sid, username, email));

  // Finishing reauth with an invalid username should fail.
  CComBSTR error2;
  ASSERT_NE(S_OK, cred->OnUserAuthenticated(invalid_signin_result, &error2));

  ASSERT_EQ(S_OK, cred->Terminate());

  EXPECT_EQ(0u, provider.username().Length());
  EXPECT_EQ(0u, provider.password().Length());
  EXPECT_EQ(0u, provider.sid().Length());
  EXPECT_NE(0u, error2.Length());
  EXPECT_EQ(FALSE, provider.credentials_changed_fired());
}

class GcpReauthCredentialRunnableTest : public ::testing::Test {
 protected:
  ~GcpReauthCredentialRunnableTest() override;

  void SetUp() override;

  FakeGlsRunHelper* run_helper() { return &run_helper_; }

 private:
  FakeGlsRunHelper run_helper_;
};

GcpReauthCredentialRunnableTest::~GcpReauthCredentialRunnableTest() = default;

void GcpReauthCredentialRunnableTest::SetUp() {
  run_helper_.SetUp();
}

TEST_F(GcpReauthCredentialRunnableTest, NormalReauth) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;
  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth.
  OSUserManager* manager = OSUserManager::Get();
  CComBSTR sid;
  DWORD error;
  ASSERT_EQ(S_OK, manager->AddUser(username, password, full_name, L"comment",
                                   true, &sid, &error));

  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK,
            CreateReauthCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, cred->QueryInterface(IID_IReauthCredential,
                                       reinterpret_cast<void**>(&reauth)));

  ASSERT_EQ(S_OK, reauth->SetUserInfo(sid, username, email));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK,
            test->SetGlsEmailAddress(test_data_storage.GetSuccessEmail()));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_EQ(S_OK, gaia_cred->Terminate());

  // Check that values were propagated to the provider.
  EXPECT_EQ(username, provider.username());
  EXPECT_EQ(password, provider.password());
  EXPECT_EQ(sid, provider.sid());
  EXPECT_EQ(TRUE, provider.credentials_changed_fired());

  ASSERT_STREQ(test->GetErrorText(), NULL);
}

}  // namespace credential_provider

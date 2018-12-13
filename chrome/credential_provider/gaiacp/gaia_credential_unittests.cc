// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

#include "base/json/json_writer.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpGaiaCredentialTest : public ::testing::Test {
 protected:
  GcpGaiaCredentialTest();

  FakeGaiaCredentialProvider* provider() { return &provider_; }
  BSTR signin_result() { return signin_result_; }

  CComBSTR MakeSigninResult(const std::string& password);

 private:
  FakeGaiaCredentialProvider provider_;
  CComBSTR signin_result_;

  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
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

  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
                      nullptr, IID_IGaiaCredential, (void**)&cred));
  ASSERT_EQ(S_OK, cred->Initialize(provider()));

  CComBSTR error;
  ASSERT_EQ(S_OK, cred->OnUserAuthenticated(signin_result(), &error));
  ASSERT_TRUE(provider()->credentials_changed_fired());
}

TEST_F(GcpGaiaCredentialTest, OnUserAuthenticated_SamePassword) {
  USES_CONVERSION;

  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
                      nullptr, IID_IGaiaCredential, (void**)&cred));
  ASSERT_EQ(S_OK, cred->Initialize(provider()));

  CComBSTR error;
  ASSERT_EQ(S_OK, cred->OnUserAuthenticated(signin_result(), &error));
  CComBSTR first_sid = provider()->sid();

  // Finishing with the same username+password should succeed.
  CComBSTR error2;
  ASSERT_EQ(S_OK, cred->OnUserAuthenticated(signin_result(), &error2));
  ASSERT_TRUE(provider()->credentials_changed_fired());
  ASSERT_EQ(first_sid, provider()->sid());
}

TEST_F(GcpGaiaCredentialTest, OnUserAuthenticated_DiffPassword) {
  USES_CONVERSION;

  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
                      nullptr, IID_IGaiaCredential, (void**)&cred));
  ASSERT_EQ(S_OK, cred->Initialize(provider()));

  CComBSTR error;
  ASSERT_EQ(S_OK, cred->OnUserAuthenticated(signin_result(), &error));
  CComBSTR first_sid = provider()->sid();

  CComBSTR new_signin_result = MakeSigninResult("password2");

  // Finishing with the same username but different password should mark
  // the password as stale and not fire the credentials changed event.
  ASSERT_EQ(S_OK, cred->OnUserAuthenticated(new_signin_result, &error));
  ASSERT_FALSE(provider()->credentials_changed_fired());
  ASSERT_EQ(first_sid, provider()->sid());
}

}  // namespace credential_provider

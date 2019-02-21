// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>
#include <credentialprovider.h>

#include <tuple>

#include "base/synchronization/waitable_event.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/token_handle_validator.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpCredentialProviderTest : public ::testing::Test {
 public:
  void CreateGCPWUser(const wchar_t* username,
                      const wchar_t* email,
                      const wchar_t* password,
                      const wchar_t* fullname,
                      const wchar_t* comment,
                      const wchar_t* gaia_id,
                      BSTR* sid) {
    ASSERT_EQ(S_OK,
              fake_os_user_manager_.CreateTestOSUser(
                  username, password, fullname, comment, gaia_id, email, sid));
  }

  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }

 private:
  void SetUp() override;

  registry_util::RegistryOverrideManager registry_override_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

void GcpCredentialProviderTest::SetUp() {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
}

TEST_F(GcpCredentialProviderTest, Basic) {
  CComPtr<IGaiaCredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_IGaiaCredentialProvider, (void**)&provider));
}

TEST_F(GcpCredentialProviderTest, SetUserArray_NoGaiaUsers) {
  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(
      S_OK,
      CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
          nullptr, IID_ICredentialProviderSetUserArray, (void**)&user_array));

  FakeCredentialProviderUserArray array;
  array.AddUser(L"sid", L"username");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK, user_array.QueryInterface(&provider));

  // There should be no credentials. Only users with the requisite registry
  // entry will be counted.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  EXPECT_EQ(0u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);
}

TEST_F(GcpCredentialProviderTest, CpusLogon) {
  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_ICredentialProvider, (void**)&provider));

  // Start process for logon screen.
  ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

  // Give list of users visible on welcome screen.
  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(S_OK, provider.QueryInterface(&user_array));
  FakeCredentialProviderUserArray array;
  array.AddUser(L"sid1", L"username1");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

  // Check credentials.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));
  CComPtr<IGaiaCredential> gaia_cred;
  EXPECT_EQ(S_OK, cred.QueryInterface(&gaia_cred));

  // Get fields.
  DWORD field_count;
  ASSERT_EQ(S_OK, provider->GetFieldDescriptorCount(&field_count));
  EXPECT_EQ(5u, field_count);

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

TEST_F(GcpCredentialProviderTest, CpusUnlock) {
  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_ICredentialProvider, (void**)&provider));

  // Start process for logon screen.
  ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_UNLOCK_WORKSTATION, 0));

  // Give list of users visible on welcome screen.
  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(S_OK, provider.QueryInterface(&user_array));
  FakeCredentialProviderUserArray array;
  array.AddUser(L"sid1", L"username1");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

  // Check credentials.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));
  CComPtr<IGaiaCredential> gaia_cred;
  EXPECT_EQ(S_OK, cred.QueryInterface(&gaia_cred));

  // Get fields.
  DWORD field_count;
  ASSERT_EQ(S_OK, provider->GetFieldDescriptorCount(&field_count));
  EXPECT_EQ(5u, field_count);

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

TEST_F(GcpCredentialProviderTest, AddPersonAfterUserRemove) {
  FakeTokenHandleValidator token_handle_validator;
  FakeInternetAvailabilityChecker internet_checker;

  // Set up such that MDM is enabled, mulit-users is not, and a user already
  // exists.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));

  const wchar_t kDummyUsername[] = L"username";
  const wchar_t kDummyPassword[] = L"password";
  CComBSTR sid;
  CreateGCPWUser(kDummyUsername, L"foo@gmail.com", kDummyPassword, L"Full Name",
                 L"Comment", L"gaia-id", &sid);

  {
    CComPtr<ICredentialProvider> provider;
    ASSERT_EQ(S_OK,
              CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                  nullptr, IID_ICredentialProvider, (void**)&provider));
    ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

    // In this case no credential should be returned.
    DWORD count;
    DWORD default_index;
    BOOL autologon;
    ASSERT_EQ(S_OK,
              provider->GetCredentialCount(&count, &default_index, &autologon));
    ASSERT_EQ(0u, count);
  }

  // Delete the OS user.  At this point, info in the HKLM registry about this
  // user will still exist.  However it gets properly cleaned up when the token
  // validator starts its refresh of token handle validity.
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->RemoveUser(kDummyUsername, kDummyPassword));

  // Start token handle refresh threads.
  token_handle_validator.StartRefreshingTokenHandleValidity();
  {
    CComPtr<ICredentialProvider> provider;
    ASSERT_EQ(S_OK,
              CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                  nullptr, IID_ICredentialProvider, (void**)&provider));
    ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

    // This time a credential should be returned.
    DWORD count;
    DWORD default_index;
    BOOL autologon;
    ASSERT_EQ(S_OK,
              provider->GetCredentialCount(&count, &default_index, &autologon));
    ASSERT_EQ(1u, count);
  }
}

// Tests the effect of the MDM settings on the credential provider.
// Parameters:
//    bool: whether the MDM URL is configured.
//    int: whether multi-users is supported:
//        0: set registry to 0
//        1: set registry to 1
//        2: don't set at all
//    bool: whether an existing user exists.
class GcpCredentialProviderMdmTest
    : public GcpCredentialProviderTest,
      public testing::WithParamInterface<std::tuple<bool, int, bool>> {};

TEST_P(GcpCredentialProviderMdmTest, Basic) {
  const bool config_mdm_url = std::get<0>(GetParam());
  const int supports_multi_users = std::get<1>(GetParam());
  const bool user_exists = std::get<2>(GetParam());
  const DWORD expected_credential_count =
      config_mdm_url && supports_multi_users != 1 && user_exists ? 0 : 1;

  if (config_mdm_url)
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));

  if (supports_multi_users != 2) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser,
                                            supports_multi_users));
  }

  if (user_exists) {
    CComBSTR sid;
    CreateGCPWUser(L"username", L"foo@gmail.com", L"password", L"Full Name",
                   L"Comment", L"gaia-id", &sid);
  }

  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_ICredentialProvider, (void**)&provider));

  // Start process for logon screen.
  ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(expected_credential_count, count);
}

INSTANTIATE_TEST_SUITE_P(GcpCredentialProviderMdmTest,
                         GcpCredentialProviderMdmTest,
                         ::testing::Combine(testing::Bool(),
                                            testing::Range(0, 3),
                                            testing::Bool()));

// Check that reauth credentials only exist when the token handle for the
// associated user is no longer valid and internet is available.
// Parameters are:
// 1. bool - does the fake user have a token handle set.
// 2. bool - is the token handle for the fake user valid (i.e. the fetch of
// the token handle info from win_http_url_fetcher returns a valid json).
// 3. bool - is internet available.

class GcpCredentialProviderWithGaiaUsersTest
    : public GcpCredentialProviderTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  FakeWinHttpUrlFetcherFactory* fake_http_url_fetcher_factory() {
    return &fake_http_url_fetcher_factory_;
  }

 private:
  FakeWinHttpUrlFetcherFactory fake_http_url_fetcher_factory_;
};

TEST_P(GcpCredentialProviderWithGaiaUsersTest, ReauthCredentialTest) {
  const bool has_token_handle = std::get<0>(GetParam());
  const bool valid_token_handle = std::get<1>(GetParam());
  const bool has_internet = std::get<2>(GetParam());
  FakeTokenHandleValidator token_handle_validator;
  FakeInternetAvailabilityChecker internet_checker(
      has_internet ? FakeInternetAvailabilityChecker::kHicForceYes
                   : FakeInternetAvailabilityChecker::kHicForceNo);

  CComBSTR sid;
  CreateGCPWUser(L"username", L"foo@gmail.com", L"password", L"Full Name",
                 L"Comment", L"gaia-id", &sid);

  if (!has_token_handle)
    ASSERT_EQ(S_OK, SetUserProperty((BSTR)sid, kUserTokenHandle, L""));

  // Token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(TokenHandleValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(),
      valid_token_handle ? "{\"expires_in\":1}" : "{}");

  // Start token handle refresh threads.
  token_handle_validator.StartRefreshingTokenHandleValidity();

  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(
      S_OK,
      CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
          nullptr, IID_ICredentialProviderSetUserArray, (void**)&user_array));

  FakeCredentialProviderUserArray array;
  array.AddUser(OLE2CW(sid), L"username");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK, user_array.QueryInterface(&provider));

  bool should_reauth_user =
      has_internet && (!has_token_handle || !valid_token_handle);

  // Check if there is a IReauthCredential depending on the state of the token
  // handle.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  // Since SetUsageScenario was not called on the provider, only the user
  // associated credentials will be created so only expect 0 or 1 credentials
  // in this test depending on if the user needs to reauth.
  ASSERT_EQ(should_reauth_user ? 1u : 0u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  if (should_reauth_user) {
    CComPtr<ICredentialProviderCredential> cred;
    ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));
    CComPtr<IReauthCredential> reauth;
    EXPECT_EQ(S_OK, cred.QueryInterface(&reauth));
  }
}

INSTANTIATE_TEST_CASE_P(,
                        GcpCredentialProviderWithGaiaUsersTest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Bool(),
                                           ::testing::Bool()));

}  // namespace credential_provider

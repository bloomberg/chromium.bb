// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>
#include <credentialprovider.h>

#include <tuple>

#include "base/synchronization/waitable_event.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpCredentialProviderTest : public ::testing::Test {
 protected:
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
  FakeWinHttpUrlFetcherFactory* fake_http_url_fetcher_factory() {
    return &fake_http_url_fetcher_factory_;
  }

  void SetUp() override;

 private:
  registry_util::RegistryOverrideManager registry_override_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
  FakeWinHttpUrlFetcherFactory fake_http_url_fetcher_factory_;
};

void GcpCredentialProviderTest::SetUp() {
  InitializeRegistryOverrideForTesting(&registry_override_);
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

  // Check credentials. None should be available because the anonymous
  // credential is not allowed during an unlock scenario.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(0u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  // Get fields.
  DWORD field_count;
  ASSERT_EQ(S_OK, provider->GetFieldDescriptorCount(&field_count));
  EXPECT_EQ(5u, field_count);

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

TEST_F(GcpCredentialProviderTest, AddPersonAfterUserRemove) {
  FakeAssociatedUserValidator associated_user_validator;
  FakeInternetAvailabilityChecker internet_checker;

  // Set up such that MDM is enabled, mulit-users is not, and a user already
  // exists.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  GoogleMdmEnrolledStatusForTesting forced_status(true);

  const wchar_t kDummyUsername[] = L"username";
  const wchar_t kDummyPassword[] = L"password";
  CComBSTR sid;
  CreateGCPWUser(kDummyUsername, L"foo@gmail.com", kDummyPassword, L"Full Name",
                 L"Comment", L"gaia-id", &sid);

  // Start token handle refresh threads.
  associated_user_validator.StartRefreshingTokenHandleValidity();

  {
    CComPtr<ICredentialProvider> provider;
    ASSERT_EQ(S_OK,
              CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                  nullptr, IID_ICredentialProvider, (void**)&provider));
    ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

    // Empty user array.
    CComPtr<ICredentialProviderSetUserArray> user_array;
    ASSERT_EQ(S_OK, provider.QueryInterface(&user_array));
    FakeCredentialProviderUserArray array;
    ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

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

  {
    CComPtr<ICredentialProvider> provider;
    ASSERT_EQ(S_OK,
              CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                  nullptr, IID_ICredentialProvider, (void**)&provider));
    ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

    // Empty user array.
    CComPtr<ICredentialProviderSetUserArray> user_array;
    ASSERT_EQ(S_OK, provider.QueryInterface(&user_array));
    FakeCredentialProviderUserArray array;
    ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

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
  FakeAssociatedUserValidator associated_user_validator;
  FakeInternetAvailabilityChecker internet_checker;

  const bool config_mdm_url = std::get<0>(GetParam());
  const int supports_multi_users = std::get<1>(GetParam());
  const bool user_exists = std::get<2>(GetParam());
  const DWORD expected_credential_count =
      config_mdm_url && supports_multi_users != 1 && user_exists ? 0 : 1;

  bool mdm_enrolled = false;
  if (config_mdm_url) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
    mdm_enrolled = true;
  }

  GoogleMdmEnrolledStatusForTesting forced_status(mdm_enrolled);

  if (supports_multi_users != 2) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser,
                                            supports_multi_users));
  }

  if (user_exists) {
    CComBSTR sid;
    CreateGCPWUser(L"username", L"foo@gmail.com", L"password", L"Full Name",
                   L"Comment", L"gaia-id", &sid);
  }

  // Valid token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{\"expires_in\":1}");

  associated_user_validator.StartRefreshingTokenHandleValidity();

  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_ICredentialProvider, (void**)&provider));

  // Start process for logon screen.
  ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

  // Empty user array.
  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(S_OK, provider.QueryInterface(&user_array));
  FakeCredentialProviderUserArray array;
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

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
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {};

TEST_P(GcpCredentialProviderWithGaiaUsersTest, ReauthCredentialTest) {
  const bool has_token_handle = std::get<0>(GetParam());
  const bool valid_token_handle = std::get<1>(GetParam());
  const bool has_internet = std::get<2>(GetParam());
  FakeAssociatedUserValidator associated_user_validator;
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
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(),
      valid_token_handle ? "{\"expires_in\":1}" : "{}");

  // Start token handle refresh threads.
  associated_user_validator.StartRefreshingTokenHandleValidity();

  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(
      S_OK,
      CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
          nullptr, IID_ICredentialProviderSetUserArray, (void**)&user_array));

  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK, user_array.QueryInterface(&provider));

  ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

  FakeCredentialProviderUserArray array;
  array.AddUser(OLE2CW(sid), L"username");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  bool should_reauth_user =
      has_internet && (!has_token_handle || !valid_token_handle);

  // Check if there is a IReauthCredential depending on the state of the token
  // handle.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  // There should always be the anonymous credential and potentially a reauth
  // credential.
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(should_reauth_user ? 2u : 1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  if (should_reauth_user) {
    CComPtr<ICredentialProviderCredential> cred;
    ASSERT_EQ(S_OK, provider->GetCredentialAt(1, &cred));
    CComPtr<IReauthCredential> reauth;
    EXPECT_EQ(S_OK, cred.QueryInterface(&reauth));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         GcpCredentialProviderWithGaiaUsersTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Check that the correct reauth credential type is created based on various
// policy settings as well as usage scenarios.
// Parameters are:
// 1. bool - are the users' token handles still valid.
// 2. CREDENTIAL_PROVIDER_USAGE_SCENARIO - the usage scenario.
// 3. bool - is the other user tile available.
// 4. bool - is machine enrolled to mdm.
// 5. bool - is the second user locking the system.
class GcpCredentialProviderAvailableCredentialsTest
    : public GcpCredentialProviderTest,
      public ::testing::WithParamInterface<
          std::tuple<bool,
                     CREDENTIAL_PROVIDER_USAGE_SCENARIO,
                     bool,
                     bool,
                     bool>> {
 protected:
  void SetUp() override;
};

void GcpCredentialProviderAvailableCredentialsTest::SetUp() {
  GcpCredentialProviderTest::SetUp();
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
}

TEST_P(GcpCredentialProviderAvailableCredentialsTest, AvailableCredentials) {
  FakeAssociatedUserValidator associated_user_validator;
  FakeInternetAvailabilityChecker internet_checker;
  FakeCredentialProviderUserArray array;

  const bool valid_token_handles = std::get<0>(GetParam());
  const CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus = std::get<1>(GetParam());
  const bool other_user_tile_available = std::get<2>(GetParam());
  const bool enrolled_to_mdm = std::get<3>(GetParam());
  const bool second_user_locking_system = std::get<4>(GetParam());

  GoogleMdmEnrolledStatusForTesting forced_status(enrolled_to_mdm);

  if (other_user_tile_available)
    array.SetAccountOptions(CPAO_EMPTY_LOCAL);

  CComBSTR first_sid;
  constexpr wchar_t first_username[] = L"username";
  CreateGCPWUser(first_username, L"foo@gmail.com", L"password", L"Full Name",
                 L"Comment", L"gaia-id", &first_sid);

  CComBSTR second_sid;
  constexpr wchar_t second_username[] = L"username2";
  CreateGCPWUser(second_username, L"foo2@gmail.com", L"password", L"Full Name",
                 L"Comment", L"gaia-id2", &second_sid);

  // Token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(),
      valid_token_handles ? "{\"expires_in\":1}" : "{}");

  // Start token handle refresh threads.
  associated_user_validator.StartRefreshingTokenHandleValidity();

  // Lock users as needed based on the validity of their token handles.
  associated_user_validator.DenySigninForUsersWithInvalidTokenHandles(cpus);

  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(
      S_OK,
      CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
          nullptr, IID_ICredentialProviderSetUserArray, (void**)&user_array));
  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK, user_array.QueryInterface(&provider));

  ASSERT_EQ(S_OK, provider->SetUsageScenario(cpus, 0));

  // If other user tile is available, no users are passed to the provider
  if (!other_user_tile_available) {
    // All users are shown if the usage is not for unlocking the workstation or
    // if fast user switching is enabled.
    bool all_users_shown = cpus != CPUS_UNLOCK_WORKSTATION;
    // Normally, the user with invalid token handles would be removed from
    // the user array except if not all the users are shown. In this case,
    // the user that locked the system is always sent in the user array.
    if (!associated_user_validator.IsUserAccessBlocked(OLE2W(first_sid)) ||
        (!all_users_shown && !second_user_locking_system)) {
      array.AddUser(OLE2CW(first_sid), first_username);
    }
    if (!associated_user_validator.IsUserAccessBlocked(OLE2W(first_sid)) ||
        (!all_users_shown && second_user_locking_system)) {
      array.AddUser(OLE2CW(second_sid), second_username);
    }
  }

  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  // Check the correct number of credentials are created.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));

  DWORD expected_credentials = 0;
  if (other_user_tile_available) {
    expected_credentials = 1;
  } else if (cpus != CPUS_UNLOCK_WORKSTATION) {
    expected_credentials = valid_token_handles && enrolled_to_mdm ? 0 : 2;
  } else {
    expected_credentials = valid_token_handles && enrolled_to_mdm ? 0 : 1;
  }

  ASSERT_EQ(expected_credentials, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  if (expected_credentials == 0)
    return;

  CComPtr<ICredentialProviderCredential> cred;
  CComPtr<ICredentialProviderCredential2> cred2;
  CComPtr<IReauthCredential> reauth;
  // Other user tile is shown, we should only create the anonymous tile as a
  // ICredentialProviderCredential2 so that it is added to the "Other User"
  // tile.
  if (other_user_tile_available) {
    EXPECT_EQ(S_OK, provider->GetCredentialAt(0, &cred));
    EXPECT_EQ(S_OK, cred.QueryInterface(&cred2));
    // Not unlocking workstation: all the credentials should be shown as
    // anonymous reauth credentials (i.e. must not expose
    // ICredentialProviderCredential2) since they all the users have expired
    // token handles and need to reauth.
  } else if (cpus != CPUS_UNLOCK_WORKSTATION) {
    EXPECT_EQ(S_OK, provider->GetCredentialAt(0, &cred));
    EXPECT_EQ(S_OK, cred.QueryInterface(&reauth));
    EXPECT_NE(S_OK, cred.QueryInterface(&cred2));

    EXPECT_EQ(S_OK, provider->GetCredentialAt(1, &cred));
    EXPECT_EQ(S_OK, cred.QueryInterface(&reauth));
    EXPECT_NE(S_OK, cred.QueryInterface(&cred2));
  } else {
    // Only the user who locked the computer should be returned as a credential
    // and it should be a ICredentialProviderCredential2 with the correct sid.
    EXPECT_EQ(S_OK, provider->GetCredentialAt(0, &cred));
    EXPECT_EQ(S_OK, cred.QueryInterface(&reauth));
    EXPECT_EQ(S_OK, cred.QueryInterface(&cred2));

    wchar_t* sid;
    EXPECT_EQ(S_OK, cred2->GetUserSid(&sid));
    EXPECT_EQ(second_user_locking_system ? second_sid : first_sid,
              CComBSTR(W2COLE(sid)));

    // In the case that a real CReauthCredential is created, we expect that this
    // credential will set the default credential provider for the user tile.
    wchar_t guid_in_wchar[64];
    ::StringFromGUID2(CLSID_GaiaCredentialProvider, guid_in_wchar,
                      base::size(guid_in_wchar));

    wchar_t guid_in_registry[64];
    ULONG length = base::size(guid_in_registry) * sizeof(guid_in_registry[0]);
    EXPECT_EQ(S_OK, GetMachineRegString(kLogonUiUserTileRegKey, sid,
                                        guid_in_registry, &length));
    EXPECT_EQ(base::string16(guid_in_wchar), base::string16(guid_in_registry));
    ::CoTaskMemFree(sid);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GcpCredentialProviderAvailableCredentialsTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(CPUS_UNLOCK_WORKSTATION, CPUS_LOGON),
                       ::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Bool()));

}  // namespace credential_provider

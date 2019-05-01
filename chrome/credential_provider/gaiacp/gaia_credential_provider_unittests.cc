// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>
#include <credentialprovider.h>

#include <tuple>

#include "base/synchronization/waitable_event.h"
#include "base/win/win_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/auth_utils.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"

namespace credential_provider {

namespace testing {

class GcpCredentialProviderTest : public GlsRunnerTestBase {};

TEST_F(GcpCredentialProviderTest, Basic) {
  CComPtr<IGaiaCredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_IGaiaCredentialProvider, (void**)&provider));
}

TEST_F(GcpCredentialProviderTest, SetUserArray_NoGaiaUsers) {
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  CComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  // There should only be the anonymous credential. Only users with the
  // requisite registry entry will be counted.
  EXPECT_EQ(1u, count);

  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));

  CComPtr<ICredentialProviderCredential2> cred2;
  ASSERT_NE(S_OK, cred.QueryInterface(&cred2));

  CComPtr<IReauthCredential> reauth_cred;
  ASSERT_NE(S_OK, cred.QueryInterface(&reauth_cred));
}

TEST_F(GcpCredentialProviderTest, CpusLogon) {
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  CComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  // There should only be the anonymous credential. Only users with the
  // requisite registry entry will be counted.
  EXPECT_EQ(1u, count);

  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));

  CComPtr<ICredentialProviderCredential2> cred2;
  ASSERT_NE(S_OK, cred.QueryInterface(&cred2));

  CComPtr<IReauthCredential> reauth_cred;
  ASSERT_NE(S_OK, cred.QueryInterface(&reauth_cred));
}

TEST_F(GcpCredentialProviderTest, CpusUnlock) {
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  CComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  SetUsageScenario(CPUS_UNLOCK_WORKSTATION);
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  // Check credentials. None should be available because the anonymous
  // credential is not allowed during an unlock scenario.
  ASSERT_EQ(0u, count);
}

TEST_F(GcpCredentialProviderTest, AutoLogonAfterUserRefresh) {
  USES_CONVERSION;
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ICredentialProvider> provider = created_provider();

  CComPtr<IGaiaCredentialProvider> gaia_provider;
  ASSERT_EQ(S_OK, provider.QueryInterface(&gaia_provider));

  // Notify that user access is denied to fake a forced recreation of the users.
  CComPtr<ICredentialUpdateEventsHandler> update_handler;
  ASSERT_EQ(S_OK, provider.QueryInterface(&update_handler));
  update_handler->UpdateCredentialsIfNeeded(true);

  // Credential changed event should have been received.
  EXPECT_TRUE(fake_provider_events()->CredentialsChangedReceived());
  fake_provider_events()->ResetCredentialsChangedReceived();

  // At the same time notify that a user has authenticated and requires a
  // sign in.
  {
    // Temporary locker to prevent DCHECKs in OnUserAuthenticated
    AssociatedUserValidator::ScopedBlockDenyAccessUpdate deny_update_locker(
        AssociatedUserValidator::Get());
    ASSERT_EQ(S_OK, gaia_provider->OnUserAuthenticated(
                        cred, CComBSTR(L"username"), CComBSTR(L"password"), sid,
                        true));
  }

  // No credential changed should have been signalled here.
  EXPECT_FALSE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return back the same credential that was just
  // auto logged on.

  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(0u, default_index);
  EXPECT_TRUE(autologon);

  CComPtr<ICredentialProviderCredential> auto_logon_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &auto_logon_cred));
  EXPECT_TRUE(auto_logon_cred.IsEqualObject(cred));

  // The next call to GetCredentialCount should return re-created credentials.

  // Fake an update request with no access changes. The pending user refresh
  // should be queued.
  update_handler->UpdateCredentialsIfNeeded(false);

  // Credential changed event should have been received.
  EXPECT_TRUE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return new credentials with no auto logon.
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  CComPtr<ICredentialProviderCredential> new_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &new_cred));
  EXPECT_FALSE(new_cred.IsEqualObject(cred));

  // Another request to refresh the credentials should yield no credential
  // changed event or refresh of credentials.
  fake_provider_events()->ResetCredentialsChangedReceived();

  update_handler->UpdateCredentialsIfNeeded(false);

  // No credential changed event should have been received.
  EXPECT_FALSE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return the same credentials with no change.
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  CComPtr<ICredentialProviderCredential> unchanged_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &unchanged_cred));
  EXPECT_TRUE(new_cred.IsEqualObject(unchanged_cred));
}

TEST_F(GcpCredentialProviderTest, AutoLogonBeforeUserRefresh) {
  USES_CONVERSION;
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ICredentialProvider> provider = created_provider();
  CComPtr<IGaiaCredentialProvider> gaia_provider;
  ASSERT_EQ(S_OK, provider.QueryInterface(&gaia_provider));

  CComPtr<ICredentialUpdateEventsHandler> update_handler;
  ASSERT_EQ(S_OK, provider.QueryInterface(&update_handler));

  // Notify user auto logon first and then notify user access denied to ensure
  // that auto logon always has precedence over user access denied.
  {
    // Temporary locker to prevent DCHECKs in OnUserAuthenticated
    AssociatedUserValidator::ScopedBlockDenyAccessUpdate deny_update_locker(
        AssociatedUserValidator::Get());
    ASSERT_EQ(S_OK, gaia_provider->OnUserAuthenticated(
                        cred, CComBSTR(L"username"), CComBSTR(L"password"), sid,
                        true));
  }

  // Credential changed event should have been received.
  EXPECT_TRUE(fake_provider_events()->CredentialsChangedReceived());
  fake_provider_events()->ResetCredentialsChangedReceived();

  // Notify that user access is denied. This should not cause a credential
  // changed since an event was already processed.
  update_handler->UpdateCredentialsIfNeeded(true);

  // No credential changed should have been signalled here.
  EXPECT_FALSE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return back the same credential that was just
  // auto logged on.
  DWORD count;
  DWORD default_index;
  BOOL autologon;

  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(0u, default_index);
  EXPECT_TRUE(autologon);

  CComPtr<ICredentialProviderCredential> auto_logon_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &auto_logon_cred));
  EXPECT_TRUE(auto_logon_cred.IsEqualObject(cred));

  // The next call to GetCredentialCount should return re-created credentials.

  // Fake an update request with no access changes. The pending user refresh
  // should be queued.
  update_handler->UpdateCredentialsIfNeeded(false);

  // Credential changed event should have been received.
  EXPECT_TRUE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return new credentials with no auto logon.
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  CComPtr<ICredentialProviderCredential> new_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &new_cred));
  EXPECT_FALSE(new_cred.IsEqualObject(cred));

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

TEST_F(GcpCredentialProviderTest, AddPersonAfterUserRemove) {
  // Set up such that MDM is enabled, mulit-users is not, and a user already
  // exists.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  GoogleMdmEnrolledStatusForTesting forced_status(true);

  const wchar_t kDummyUsername[] = L"username";
  const wchar_t kDummyPassword[] = L"password";
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDummyUsername, kDummyPassword, L"full name", L"comment",
                      L"gaia-id", L"foo@gmail.com", &sid));

  {
    CComPtr<ICredentialProviderCredential> cred;
    CComPtr<ICredentialProvider> provider;
    DWORD count = 0;
    ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

    // In this case no credential should be returned.
    ASSERT_EQ(0u, count);

    // Release the CP so we can create another one.
    ASSERT_EQ(S_OK, ReleaseProvider());
  }

  // Delete the OS user.  At this point, info in the HKLM registry about this
  // user will still exist.  However it gets properly cleaned up when the token
  // validator starts its refresh of token handle validity.
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->RemoveUser(kDummyUsername, kDummyPassword));

  {
    CComPtr<ICredentialProvider> provider;
    DWORD count = 0;
    ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

    // This time a credential should be returned.
    ASSERT_EQ(1u, count);

    // And this credential should be the anonymous one.
    CComPtr<ICredentialProviderCredential> cred;
    ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));

    CComPtr<ICredentialProviderCredential2> cred2;
    ASSERT_NE(S_OK, cred.QueryInterface(&cred2));

    CComPtr<IReauthCredential> reauth_cred;
    ASSERT_NE(S_OK, cred.QueryInterface(&reauth_cred));

    // Release the CP.
    ASSERT_EQ(S_OK, provider->UnAdvise());
  }
}

class GcpCredentialProviderExecutionTest : public GlsRunnerTestBase {};

TEST_F(GcpCredentialProviderExecutionTest, UnAdviseDuringGls) {
  USES_CONVERSION;

  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  // This event is merely used to keep the gls running while it is killed by
  // Terminate().
  constexpr wchar_t kStartGlsEventName[] = L"UnAdviseDuringGls_Signal";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/true));

  // Release the provider which should also Terminate the credential that
  // was created.
  ReleaseProvider();
}

// Tests auto logon enabled when set serialization is called.
// Parameters:
// 1. bool: are the users' token handles still valid.
// 2. CREDENTIAL_PROVIDER_USAGE_SCENARIO - the usage scenario.
class GcpCredentialProviderSetSerializationTest
    : public GcpCredentialProviderTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, CREDENTIAL_PROVIDER_USAGE_SCENARIO>> {};

TEST_P(GcpCredentialProviderSetSerializationTest, CheckAutoLogon) {
  const bool valid_token_handles = std::get<0>(GetParam());
  const CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus = std::get<1>(GetParam());

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  GoogleMdmEnrolledStatusForTesting forced_status(true);

  CComBSTR first_sid;
  constexpr wchar_t first_username[] = L"username";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      first_username, L"password", L"full name", L"comment",
                      L"gaia-id", L"foo@gmail.com", &first_sid));

  CComBSTR second_sid;
  constexpr wchar_t second_username[] = L"username2";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      second_username, L"password", L"Full Name", L"Comment",
                      L"gaia-id2", L"foo2@gmail.com", &second_sid));

  // Build a dummy authentication buffer that can be passed to SetSerialization.
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  base::string16 local_domain = OSUserManager::GetLocalDomain();
  base::string16 serialization_username = second_username;
  base::string16 serialization_password = L"password";
  std::vector<wchar_t> dummy_domain(
      local_domain.c_str(), local_domain.c_str() + local_domain.size() + 1);
  std::vector<wchar_t> dummy_username(
      serialization_username.c_str(),
      serialization_username.c_str() + serialization_username.size() + 1);
  std::vector<wchar_t> dummy_password(
      serialization_password.c_str(),
      serialization_password.c_str() + serialization_password.size() + 1);
  ASSERT_EQ(S_OK, BuildCredPackAuthenticationBuffer(
                      &dummy_domain[0], &dummy_username[0], &dummy_password[0],
                      cpus, &cpcs));

  GetAuthenticationPackageId(&cpcs.ulAuthenticationPackage);
  cpcs.clsidCredentialProvider = CLSID_GaiaCredentialProvider;

  CComPtr<ICredentialProviderCredential> cred;
  CComPtr<ICredentialProvider> provider;
  SetDefaultTokenHandleResponse(valid_token_handles
                                    ? kDefaultValidTokenHandleResponse
                                    : kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderWithRemoteCredentials(&cpcs, &provider));

  ::CoTaskMemFree(cpcs.rgbSerialization);

  // Check the correct number of credentials are created and whether autologon
  // is enabled based on the token handle validity.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));

  bool should_autologon = !valid_token_handles;
  EXPECT_EQ(valid_token_handles ? 0u : 2u, count);
  EXPECT_EQ(autologon, should_autologon);
  EXPECT_EQ(default_index,
            should_autologon ? 1 : CREDENTIAL_PROVIDER_NO_DEFAULT);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GcpCredentialProviderSetSerializationTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(CPUS_UNLOCK_WORKSTATION, CPUS_LOGON)));

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
      public ::testing::WithParamInterface<std::tuple<bool, int, bool>> {};

TEST_P(GcpCredentialProviderMdmTest, Basic) {
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
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        L"username", L"password", L"full name", L"comment",
                        L"gaia-id", L"foo@gmail.com", &sid));
  }

  CComPtr<ICredentialProviderCredential> cred;
  CComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  ASSERT_EQ(expected_credential_count, count);

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

INSTANTIATE_TEST_SUITE_P(GcpCredentialProviderMdmTest,
                         GcpCredentialProviderMdmTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Range(0, 3),
                                            ::testing::Bool()));

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
  fake_internet_checker()->SetHasInternetConnection(
      has_internet ? FakeInternetAvailabilityChecker::kHicForceYes
                   : FakeInternetAvailabilityChecker::kHicForceNo);

  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment",
                      L"gaia-id", L"foo@gmail.com", &sid));

  if (!has_token_handle)
    ASSERT_EQ(S_OK, SetUserProperty((BSTR)sid, kUserTokenHandle, L""));

  CComPtr<ICredentialProviderCredential> cred;
  CComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  SetDefaultTokenHandleResponse(valid_token_handle
                                    ? kDefaultValidTokenHandleResponse
                                    : kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  bool should_reauth_user =
      has_internet && (!has_token_handle || !valid_token_handle);

  // Check if there is a IReauthCredential depending on the state of the token
  // handle.
  ASSERT_EQ(should_reauth_user ? 2u : 1u, count);

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
  USES_CONVERSION;

  const bool valid_token_handles = std::get<0>(GetParam());
  const CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus = std::get<1>(GetParam());
  const bool other_user_tile_available = std::get<2>(GetParam());
  const bool enrolled_to_mdm = std::get<3>(GetParam());
  const bool second_user_locking_system = std::get<4>(GetParam());

  GoogleMdmEnrolledStatusForTesting forced_status(enrolled_to_mdm);

  if (other_user_tile_available)
    fake_user_array()->SetAccountOptions(CPAO_EMPTY_LOCAL);

  CComBSTR first_sid;
  constexpr wchar_t first_username[] = L"username";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      first_username, L"password", L"full name", L"comment",
                      L"gaia-id", L"foo@gmail.com", &first_sid));

  CComBSTR second_sid;
  constexpr wchar_t second_username[] = L"username2";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      second_username, L"password", L"Full Name", L"Comment",
                      L"gaia-id2", L"foo2@gmail.com", &second_sid));

  // Set the user locking the system.
  SetSidLockingWorkstation(second_user_locking_system ? OLE2CW(second_sid)
                                                      : OLE2CW(first_sid));

  CComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  SetUsageScenario(cpus);
  SetDefaultTokenHandleResponse(valid_token_handles
                                    ? kDefaultValidTokenHandleResponse
                                    : kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  // Check the correct number of credentials are created.
  DWORD expected_credentials = 0;
  if (cpus != CPUS_UNLOCK_WORKSTATION) {
    expected_credentials = valid_token_handles && enrolled_to_mdm ? 0 : 2;
    if (other_user_tile_available)
      expected_credentials += 1;
  } else {
    if (other_user_tile_available) {
      expected_credentials = 1;
    } else {
      expected_credentials = valid_token_handles && enrolled_to_mdm ? 0 : 1;
    }
  }

  ASSERT_EQ(expected_credentials, count);

  // No credentials to verify.
  if (expected_credentials == 0)
    return;

  CComPtr<ICredentialProviderCredential> cred;
  CComPtr<ICredentialProviderCredential2> cred2;
  CComPtr<IReauthCredential> reauth;

  DWORD first_non_anonymous_cred_index = 0;

  // Other user tile is shown, we should create the anonymous tile as a
  // ICredentialProviderCredential2 so that it is added to the "Other User"
  // tile.
  if (other_user_tile_available) {
    EXPECT_EQ(S_OK, provider->GetCredentialAt(first_non_anonymous_cred_index++,
                                              &cred));
    EXPECT_EQ(S_OK, cred.QueryInterface(&cred2));
  }

  // Not unlocking workstation: if there are more credentials then they should
  // all the credentials should be shown as reauth credentials since all the
  // users have expired token handles (or need mdm enrollment) and need to
  // reauth.

  if (cpus != CPUS_UNLOCK_WORKSTATION) {
    if (first_non_anonymous_cred_index < expected_credentials) {
      EXPECT_EQ(S_OK, provider->GetCredentialAt(
                          first_non_anonymous_cred_index++, &cred));
      EXPECT_EQ(S_OK, cred.QueryInterface(&reauth));
      EXPECT_EQ(S_OK, cred.QueryInterface(&cred2));

      EXPECT_EQ(S_OK, provider->GetCredentialAt(
                          first_non_anonymous_cred_index++, &cred));
      EXPECT_EQ(S_OK, cred.QueryInterface(&reauth));
      EXPECT_EQ(S_OK, cred.QueryInterface(&cred2));
    }
  } else if (!other_user_tile_available) {
    // Only the user who locked the computer should be returned as a credential
    // and it should be a ICredentialProviderCredential2 with the correct sid.
    EXPECT_EQ(S_OK, provider->GetCredentialAt(first_non_anonymous_cred_index++,
                                              &cred));
    EXPECT_EQ(S_OK, cred.QueryInterface(&reauth));
    EXPECT_EQ(S_OK, cred.QueryInterface(&cred2));

    wchar_t* sid;
    EXPECT_EQ(S_OK, cred2->GetUserSid(&sid));
    EXPECT_EQ(second_user_locking_system ? second_sid : first_sid,
              CComBSTR(W2COLE(sid)));

    // In the case that a real CReauthCredential is created, we expect that this
    // credential will set the default credential provider for the user tile.
    auto guid_string =
        base::win::String16FromGUID(CLSID_GaiaCredentialProvider);

    wchar_t guid_in_registry[64];
    ULONG length = base::size(guid_in_registry);
    EXPECT_EQ(S_OK, GetMachineRegString(kLogonUiUserTileRegKey, sid,
                                        guid_in_registry, &length));
    EXPECT_EQ(guid_string, base::string16(guid_in_registry));
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

}  // namespace testing

}  // namespace credential_provider

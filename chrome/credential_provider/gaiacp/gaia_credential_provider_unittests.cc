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
#include "base/win/win_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/associated_user_validator.h"
#include "chrome/credential_provider/gaiacp/auth_utils.h"
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
  EXPECT_EQ(FIELD_COUNT, field_count);

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
  EXPECT_EQ(FIELD_COUNT, field_count);

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

TEST_F(GcpCredentialProviderTest, AutoLogonAfterUserRefresh) {
  USES_CONVERSION;
  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_ICredentialProvider, (void**)&provider));

  CComPtr<IGaiaCredentialProvider> gaia_provider;
  ASSERT_EQ(S_OK, provider.QueryInterface(&gaia_provider));

  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"passowrd", L"Full Name", L"Comment", L"",
                      L"", &sid));
  // Start process for logon screen.
  ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

  // Give empty list of users so that only the anonymous credential is created.
  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(S_OK, provider.QueryInterface(&user_array));
  FakeCredentialProviderUserArray array;
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

  // Only the anonymous credential should exist.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  // Get the anonymous credential.
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));

  // Notify that user access is denied to fake a forced recreation of the users.
  ICredentialUpdateEventsHandler* update_handler =
      static_cast<ICredentialUpdateEventsHandler*>(
          static_cast<CGaiaCredentialProvider*>(provider.p));
  update_handler->UpdateCredentialsIfNeeded(true);

  // Credential changed event should have been received.
  EXPECT_TRUE(events.CredentialsChangedReceived());
  events.ResetCredentialsChangedReceived();

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
  EXPECT_FALSE(events.CredentialsChangedReceived());

  // GetCredentialCount should return back the same credential that was just
  // auto logged on.
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
  EXPECT_TRUE(events.CredentialsChangedReceived());

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
  events.ResetCredentialsChangedReceived();

  update_handler->UpdateCredentialsIfNeeded(false);

  // No credential changed event should have been received.
  EXPECT_FALSE(events.CredentialsChangedReceived());

  // GetCredentialCount should return the same credentials with no change.
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  CComPtr<ICredentialProviderCredential> unchanged_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &unchanged_cred));
  EXPECT_TRUE(new_cred.IsEqualObject(unchanged_cred));

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

TEST_F(GcpCredentialProviderTest, AutoLogonBeforeUserRefresh) {
  USES_CONVERSION;
  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_ICredentialProvider, (void**)&provider));

  CComPtr<IGaiaCredentialProvider> gaia_provider;
  ASSERT_EQ(S_OK, provider.QueryInterface(&gaia_provider));

  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"passowrd", L"Full Name", L"Comment", L"",
                      L"", &sid));
  // Start process for logon screen.
  ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

  // Give empty list of users so that only the anonymous credential is created.
  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(S_OK, provider.QueryInterface(&user_array));
  FakeCredentialProviderUserArray array;
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

  // Only the anonymous credential should exist.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  // Get the anonymous credential.
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));

  ICredentialUpdateEventsHandler* update_handler =
      static_cast<ICredentialUpdateEventsHandler*>(
          static_cast<CGaiaCredentialProvider*>(provider.p));

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
  EXPECT_TRUE(events.CredentialsChangedReceived());
  events.ResetCredentialsChangedReceived();

  // Notify that user access is denied. This should not cause a credential
  // changed since an event was already processed.
  update_handler->UpdateCredentialsIfNeeded(true);

  // No credential changed should have been signalled here.
  EXPECT_FALSE(events.CredentialsChangedReceived());

  // GetCredentialCount should return back the same credential that was just
  // auto logged on.
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
  EXPECT_TRUE(events.CredentialsChangedReceived());

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

    // Activate the CP.
    FakeCredentialProviderEvents events;
    ASSERT_EQ(S_OK, provider->Advise(&events, 0));

    // In this case no credential should be returned.
    DWORD count;
    DWORD default_index;
    BOOL autologon;
    ASSERT_EQ(S_OK,
              provider->GetCredentialCount(&count, &default_index, &autologon));
    ASSERT_EQ(0u, count);

    // Deactivate the CP.
    ASSERT_EQ(S_OK, provider->UnAdvise());
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

    // Activate the CP.
    FakeCredentialProviderEvents events;
    ASSERT_EQ(S_OK, provider->Advise(&events, 0));

    // This time a credential should be returned.
    DWORD count;
    DWORD default_index;
    BOOL autologon;
    ASSERT_EQ(S_OK,
              provider->GetCredentialCount(&count, &default_index, &autologon));
    ASSERT_EQ(1u, count);

    // Deactivate the CP.
    ASSERT_EQ(S_OK, provider->UnAdvise());
  }
}

// Tests auto logon enabled when set serialization is called.
// Parameters:
// 1. bool: are the users' token handles still valid.
// 2. CREDENTIAL_PROVIDER_USAGE_SCENARIO - the usage scenario.
class GcpCredentialProviderSetSerializationTest
    : public GcpCredentialProviderTest,
      public testing::WithParamInterface<
          std::tuple<bool, CREDENTIAL_PROVIDER_USAGE_SCENARIO>> {};

TEST_P(GcpCredentialProviderSetSerializationTest, CheckAutoLogon) {
  FakeAssociatedUserValidator associated_user_validator;
  FakeInternetAvailabilityChecker internet_checker;

  const bool valid_token_handles = std::get<0>(GetParam());
  const CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus = std::get<1>(GetParam());

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  GoogleMdmEnrolledStatusForTesting forced_status(true);

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

  cpcs.clsidCredentialProvider = CLSID_GaiaCredentialProvider;

  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(
      S_OK,
      CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
          nullptr, IID_ICredentialProviderSetUserArray, (void**)&user_array));
  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK, user_array.QueryInterface(&provider));

  ASSERT_EQ(S_OK, provider->SetUsageScenario(cpus, 0));

  ASSERT_EQ(S_OK, provider->SetSerialization(&cpcs));

  ::CoTaskMemFree(cpcs.rgbSerialization);

  FakeCredentialProviderUserArray array;
  array.AddUser(OLE2CW(first_sid), first_username);
  array.AddUser(OLE2CW(second_sid), second_username);

  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

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

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
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

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(expected_credential_count, count);

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
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

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

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

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
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

  // All users are shown if the usage is not for unlocking the workstation.
  bool all_users_shown = cpus != CPUS_UNLOCK_WORKSTATION;
  // If not all the users are shown, the user that locked the system is
  // the only one that is in the user array (if the other user tile is
  // not available).
  if (all_users_shown ||
      (!second_user_locking_system && !other_user_tile_available)) {
    array.AddUser(OLE2CW(first_sid), first_username);
  }
  if (all_users_shown ||
      (second_user_locking_system && !other_user_tile_available)) {
    array.AddUser(OLE2CW(second_sid), second_username);
  }

  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

  // Check the correct number of credentials are created.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));

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
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  if (expected_credentials == 0) {
    // Deactivate the CP.
    ASSERT_EQ(S_OK, provider->UnAdvise());
    return;
  }

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

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
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

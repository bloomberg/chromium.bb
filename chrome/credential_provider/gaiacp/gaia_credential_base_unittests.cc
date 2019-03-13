// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

// This class is used to implement a test credential based off only
// CGaiaCredentialBase which requires certain functions be implemented.
class ATL_NO_VTABLE CTestCredentialForBase
    : public CTestCredentialBase<CGaiaCredentialBase>,
      public CComObjectRootEx<CComMultiThreadModel> {
 public:
  DECLARE_NO_REGISTRY()

  CTestCredentialForBase();
  ~CTestCredentialForBase();

  HRESULT FinalConstruct() { return S_OK; }
  void FinalRelease() {}

 private:
  BEGIN_COM_MAP(CTestCredentialForBase)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ITestCredential)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()
};

CTestCredentialForBase::CTestCredentialForBase() = default;

CTestCredentialForBase::~CTestCredentialForBase() = default;

namespace {

HRESULT CreateCredential(ICredentialProviderCredential** credential) {
  return CComCreator<CComObject<CTestCredentialForBase>>::CreateInstance(
      nullptr, IID_ICredentialProviderCredential,
      reinterpret_cast<void**>(credential));
}

HRESULT CreateCredentialWithProvider(
    IGaiaCredentialProvider* provider,
    IGaiaCredential** gaia_credential,
    ICredentialProviderCredential** credential) {
  HRESULT hr = CreateCredential(credential);
  if (SUCCEEDED(hr)) {
    hr = (*credential)
             ->QueryInterface(IID_IGaiaCredential,
                              reinterpret_cast<void**>(gaia_credential));
    if (SUCCEEDED(hr))
      hr = (*gaia_credential)->Initialize(provider);
  }
  return hr;
}

}  // namespace

class GcpGaiaCredentialBaseTest : public GlsRunnerTestBase {};

TEST_F(GcpGaiaCredentialBaseTest, Advise) {
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredential(&cred));

  ASSERT_EQ(S_OK, cred->Advise(nullptr));
  ASSERT_EQ(S_OK, cred->UnAdvise());
}

TEST_F(GcpGaiaCredentialBaseTest, SetSelected) {
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredential(&cred));

  // A credential that has not attempted to sign in a user yet should return
  // false for |auto_login|.
  BOOL auto_login;
  ASSERT_EQ(S_OK, cred->SetSelected(&auto_login));
  ASSERT_FALSE(auto_login);
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_NoInternet) {
  FakeGaiaCredentialProvider provider;
  FakeInternetAvailabilityChecker internet_checker(
      FakeInternetAvailabilityChecker::kHicForceNo);

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcess(cred, /*succeeds=*/false));

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Start) {
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Finish) {
  FakeGaiaCredentialProvider provider;

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  // Now finish the logon.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_SUCCESS, status_icon);
  EXPECT_EQ(CPGSR_RETURN_CREDENTIAL_FINISHED, cpgsr);
  EXPECT_LT(0u, cpcs.cbSerialization);
  EXPECT_NE(nullptr, cpcs.rgbSerialization);

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  // State was not reset.
  EXPECT_TRUE(test->AreCredentialsValid());

  // Make sure a "foo" user was created.
  PSID sid;
  EXPECT_EQ(S_OK, fake_os_user_manager()->GetUserSID(
                      OSUserManager::GetLocalDomain().c_str(), kDefaultUsername,
                      &sid));
  ::LocalFree(sid);
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  wchar_t* report_status_text = nullptr;
  CREDENTIAL_PROVIDER_STATUS_ICON report_icon;
  EXPECT_EQ(S_OK, cred->ReportResult(0, 0, &report_status_text, &report_icon));
  // State was reset.
  EXPECT_FALSE(test->AreCredentialsValid());

  EXPECT_EQ(S_OK, gaia_cred->Terminate());

  // New user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Abort) {
  FakeGaiaCredentialProvider provider;

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetDefaultExitCode(kUiecAbort));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  // Now finish the logon.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_NONE, status_icon);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);
  EXPECT_EQ(nullptr, test->GetErrorText());

  EXPECT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_AssociateToMatchingAssociatedUser) {
  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToUTF16(kDefaultGaiaId), base::string16(),
                      &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  FakeGaiaCredentialProvider provider;

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  // Now finish the logon.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_SUCCESS, status_icon);
  EXPECT_EQ(CPGSR_RETURN_CREDENTIAL_FINISHED, cpgsr);
  EXPECT_LT(0u, cpcs.cbSerialization);
  EXPECT_NE(nullptr, cpcs.rgbSerialization);

  // State was not reset.
  EXPECT_TRUE(test->AreCredentialsValid());

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  wchar_t* report_status_text = nullptr;
  CREDENTIAL_PROVIDER_STATUS_ICON report_icon;
  EXPECT_EQ(S_OK, cred->ReportResult(0, 0, &report_status_text, &report_icon));
  // State was reset.
  EXPECT_FALSE(test->AreCredentialsValid());

  EXPECT_EQ(S_OK, gaia_cred->Terminate());

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_MultipleCalls) {
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr wchar_t kStartGlsEventName[] =
      L"GetSerialization_MultipleCalls_Wait";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcess(cred, /*succeeds=*/true));

  // Calling GetSerialization again while the credential is waiting for the
  // logon process should yield CPGSR_NO_CREDENTIAL_NOT_FINISHED as a
  // response.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  EXPECT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_NONE, status_icon);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Signal that the gls process can finish.
  start_event.Signal();

  ASSERT_EQ(S_OK, run_helper()->WaitForLogonProcess(cred));
  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_PasswordChangedForAssociatedUser) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Set an invalid password and try to get serialization again. Credentials
  // should still be valid but serialization is not complete.
  CComBSTR invalid_windows_password = L"a";
  test->SetWindowsPassword(invalid_windows_password);
  EXPECT_EQ(nullptr, status_text);
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Update the Windows password to be the real password created for the user.
  test->SetWindowsPassword(windows_password);
  // Sign in information should still be available.
  EXPECT_TRUE(test->GetFinalEmail().length());

  // Both Windows and Gaia credentials should be valid now
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_FALSE, test->IsWindowsPasswordValidForStoredUser());

  // Serialization should complete without any errors.
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_SUCCESS, status_icon);
  EXPECT_EQ(CPGSR_RETURN_CREDENTIAL_FINISHED, cpgsr);
  EXPECT_LT(0u, cpcs.cbSerialization);
  EXPECT_NE(nullptr, cpcs.rgbSerialization);

  // State was not reset.
  EXPECT_TRUE(test->AreCredentialsValid());
  wchar_t* report_status_text = nullptr;
  CREDENTIAL_PROVIDER_STATUS_ICON report_icon;
  EXPECT_EQ(S_OK, cred->ReportResult(0, 0, &report_status_text, &report_icon));
  // State was reset.
  EXPECT_FALSE(test->AreCredentialsValid());

  EXPECT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Cancel) {
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  // This event is merely used to keep the gls running while it is cancelled
  // through SetDeselected().
  constexpr wchar_t kStartGlsEventName[] = L"GetSerialization_Cancel_Signal";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcess(cred, /*succeeds=*/true));

  // Deselect the credential provider so that it cancels the GLS process and
  // returns.
  ASSERT_EQ(S_OK, cred->SetDeselected());

  ASSERT_EQ(S_OK, run_helper()->WaitForLogonProcess(cred));
  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  constexpr char email[] = "foo@imfl.info";

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"foo_imfl"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, NewUserDisabledThroughUsageScenario) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  provider.SetUsageScenario(CPUS_UNLOCK_WORKSTATION);

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_EQ(S_OK, gaia_cred->Terminate());

  // Check that values were not propagated to the provider.
  EXPECT_EQ(0u, provider.username().Length());
  EXPECT_EQ(0u, provider.password().Length());
  EXPECT_EQ(0u, provider.sid().Length());
  EXPECT_EQ(FALSE, provider.credentials_changed_fired());

  // Sign in should fail with an error stating that no new users can be created.
  ASSERT_STREQ(
      test->GetErrorText(),
      GetStringResource(IDS_INVALID_UNLOCK_WORKSTATION_USER_BASE).c_str());
}

TEST_F(GcpGaiaCredentialBaseTest, NewUserDisabledThroughMdm) {
  USES_CONVERSION;
  FakeAssociatedUserValidator validator;
  FakeInternetAvailabilityChecker internet_checker;

  // Enforce single user mode for MDM.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user that is already associated so when the user tries to
  // sign on and create a new user, it fails.
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo_registered", L"password", L"name", L"comment",
                      L"gaia-id-registered", base::string16(), &sid));

  FakeGaiaCredentialProvider provider;

  // Populate the associated users list, token handle validity does not matter
  // in this test.
  validator.StartRefreshingTokenHandleValidity();

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_EQ(S_OK, gaia_cred->Terminate());

  // Check that values were not propagated to the provider.
  EXPECT_EQ(0u, provider.username().Length());
  EXPECT_EQ(0u, provider.password().Length());
  EXPECT_EQ(0u, provider.sid().Length());
  EXPECT_EQ(FALSE, provider.credentials_changed_fired());

  // Sign in should fail with an error stating that no new users can be created.
  ASSERT_STREQ(test->GetErrorText(),
               GetStringResource(IDS_ADD_USER_DISALLOWED_BASE).c_str());
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUserUnlockedAfterSignin) {
  // Enforce token handle verification with user locking when the token handle
  // is not valid.
  FakeAssociatedUserValidator validator;
  FakeInternetAvailabilityChecker internet_checker;
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  GoogleMdmEnrollmentStatusForTesting force_success(true);

  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                username, L"password", L"name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Invalid token fetch result.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{}");

  // Lock the user through their token handle.
  validator.StartRefreshingTokenHandleValidity();
  validator.DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON);

  // User should have invalid token handle and be locked.
  DWORD reg_value = 0;
  EXPECT_FALSE(validator.IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(true, validator.IsUserAccessBlocked(OLE2W(sid)));
  EXPECT_EQ(S_OK,
            GetMachineRegDWORD(kWinlogonUserListRegKey, username, &reg_value));
  EXPECT_EQ(0u, reg_value);

  FakeGaiaCredentialProvider provider;

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  // Now finish the logon, this should unlock the user.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_SUCCESS, status_icon);
  EXPECT_EQ(CPGSR_RETURN_CREDENTIAL_FINISHED, cpgsr);
  EXPECT_LT(0u, cpcs.cbSerialization);
  EXPECT_NE(nullptr, cpcs.rgbSerialization);

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  EXPECT_EQ(false, validator.IsUserAccessBlocked(OLE2W(sid)));
  EXPECT_NE(S_OK,
            GetMachineRegDWORD(kWinlogonUserListRegKey, username, &reg_value));

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Gmail) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  constexpr char email[] = "bar@gmail.com";

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"bar"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Googlemail) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  constexpr char email[] = "toto@googlemail.com";

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"toto"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUsernameCharacters) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  constexpr char email[] = "a\\[]:|<>+=;?*z@gmail.com";

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"a____________z"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  constexpr char email[] = "areallylongemailadressdude@gmail.com";

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"areallylongemailadre"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong2) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  constexpr char email[] = "foo@areallylongdomaindude.com";

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"foo_areallylongdomai"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsNoAt) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  constexpr char email[] = "foo";

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"foo_gmail"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtCom) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  constexpr char email[] = "@com";

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"_com"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtDotCom) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  constexpr char email[] = "@.com";

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"_.com"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

}  // namespace testing
}  // namespace credential_provider

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/fake_gls_run_helper.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

// This class is used to implement a test credential based off only
// CGaiaCredentialBase which requires certain functions be implemented.

// This class is used to implement a test credential based off only
// CGaiaCredentialBase which requires certain
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
  return CComCreator<CComObject<testing::CTestCredentialForBase>>::
      CreateInstance(nullptr, IID_ICredentialProviderCredential,
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

class GcpGaiaCredentialBaseTest : public ::testing::Test {
 protected:
  ~GcpGaiaCredentialBaseTest() override;

  void SetUp() override;

  FakeGlsRunHelper* run_helper() { return &run_helper_; }

 private:
  FakeGlsRunHelper run_helper_;
};

GcpGaiaCredentialBaseTest::~GcpGaiaCredentialBaseTest() = default;

void GcpGaiaCredentialBaseTest::SetUp() {
  run_helper_.SetUp();
}

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
  ASSERT_EQ(S_OK, provider.SetHasInternetConnection(kHicForceNo));

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcess(cred, /*succeeds=*/false));

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Start) {
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
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

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  // Now finish the logon.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  ASSERT_EQ(nullptr, status_text);
  ASSERT_EQ(CPSI_NONE, status_icon);
  ASSERT_EQ(CPGSR_RETURN_CREDENTIAL_FINISHED, cpgsr);
  ASSERT_LT(0u, cpcs.cbSerialization);
  ASSERT_NE(nullptr, cpcs.rgbSerialization);

  // State was reset.
  ASSERT_FALSE(test->AreCredentialsValid());

  // Make sure a "foo" user was created.
  PSID sid;
  ASSERT_EQ(S_OK, run_helper()->fake_os_user_manager()->GetUserSID(
                      testing::kDefaultUsername, &sid));
  ::LocalFree(sid);

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_MultipleCalls) {
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
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

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_PasswordChanged) {
  FakeGaiaCredentialProvider provider;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  OSUserManager* manager = OSUserManager::Get();
  CComBSTR sid;
  DWORD error;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK, manager->AddUser(L"foo", (BSTR)windows_password, L"Full Name",
                                   L"comment", true, &sid, &error));

  // Start logon.
  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_TRUE(test->AreWindowsCredentialsAvailable());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  ASSERT_EQ(nullptr, status_text);
  ASSERT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  ASSERT_TRUE(test->AreWindowsCredentialsAvailable());

  // Set an invalid password and try to get serialization again. Credentials
  // should still be valid but serialization is not complete.
  CComBSTR invalid_windows_password = L"a";
  test->SetWindowsPassword(invalid_windows_password);
  ASSERT_EQ(nullptr, status_text);
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  ASSERT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Update the Windows password to be the real password created for the user.
  test->SetWindowsPassword(windows_password);

  // Both Windows and Gaia credentials should be valid now
  ASSERT_TRUE(test->AreWindowsCredentialsAvailable());
  ASSERT_TRUE(test->AreWindowsCredentialsValid());

  // Serialization should complete without any errors.
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  ASSERT_EQ(nullptr, status_text);
  ASSERT_EQ(CPSI_NONE, status_icon);
  ASSERT_EQ(CPGSR_RETURN_CREDENTIAL_FINISHED, cpgsr);
  ASSERT_LT(0u, cpcs.cbSerialization);
  ASSERT_NE(nullptr, cpcs.rgbSerialization);

  // State was reset.
  ASSERT_FALSE(test->AreCredentialsValid());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Cancel) {
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
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

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("foo@imfl.info"));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"foo_imfl"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Gmail) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("bar@gmail.com"));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"bar"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Googlemail) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("toto@googlemail.com"));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"toto"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUsernameCharacters) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("a\\[]:|<>+=;?*z@gmail.com"));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"a____________z"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK,
            test->SetGlsEmailAddress("areallylongemailadressdude@gmail.com"));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"areallylongemailadre"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong2) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("foo@areallylongdomaindude.com"));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"foo_areallylongdomai"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsNoAt) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("foo"));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"foo_gmail"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtCom) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("@com"));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"_com"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtDotCom) {
  USES_CONVERSION;
  FakeGaiaCredentialProvider provider;

  CComPtr<IGaiaCredential> gaia_cred;
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, CreateCredentialWithProvider(&provider, &gaia_cred, &cred));

  CComPtr<testing::ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("@.com"));

  ASSERT_EQ(S_OK, run_helper()->StartLogonProcessAndWait(cred));

  ASSERT_STREQ(W2COLE(L"_.com"), test->GetFinalUsername());

  ASSERT_EQ(S_OK, gaia_cred->Terminate());
}

}  // namespace testing
}  // namespace credential_provider

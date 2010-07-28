// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"
#include "base/file_version_info_win.h"
#include "chrome_frame/utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

const wchar_t kChannelName[] = L"-dev";
const wchar_t kSuffix[] = L"-fix";

TEST(UtilTests, GetModuleVersionTest) {
  HMODULE mod = GetModuleHandle(L"kernel32.dll");
  EXPECT_NE(mod, static_cast<HMODULE>(NULL));
  wchar_t path[MAX_PATH] = {0};
  GetModuleFileName(mod, path, arraysize(path));

  // Use the method that goes to disk
  scoped_ptr<FileVersionInfo> base_info(
      FileVersionInfo::CreateFileVersionInfo(path));
  EXPECT_TRUE(base_info.get() != NULL);

  // Use the method that doesn't go to disk
  uint32 low = 0, high = 0;
  EXPECT_TRUE(GetModuleVersion(mod, &high, &low));
  EXPECT_NE(high, 0u);
  EXPECT_NE(low, 0u);

  // Make sure they give the same results.
  FileVersionInfoWin* base_info_win =
      static_cast<FileVersionInfoWin*>(base_info.get());
  VS_FIXEDFILEINFO* fixed_info = base_info_win->fixed_file_info();
  EXPECT_TRUE(fixed_info != NULL);

  EXPECT_EQ(fixed_info->dwFileVersionMS, static_cast<DWORD>(high));
  EXPECT_EQ(fixed_info->dwFileVersionLS, static_cast<DWORD>(low));
}

TEST(UtilTests, HaveSameOrigin) {
  struct OriginCompare {
    const char* a;
    const char* b;
    bool same_origin;
  } test_cases[] = {
    { "", "", true },
    { "*", "*", true },
    { "*", "+", false },
    { "http://www.google.com/", "http://www.google.com/", true },
    { "http://www.google.com", "http://www.google.com/", true },
    { "http://www.google.com:80/", "http://www.google.com/", true },
    { "http://www.google.com:8080/", "http://www.google.com/", false },
    { "https://www.google.com/", "http://www.google.com/", false },
    { "http://docs.google.com/", "http://www.google.com/", false },
    { "https://www.google.com/", "https://www.google.com:443/", true },
    { "https://www.google.com/", "https://www.google.com:443", true },
  };

  for (int i = 0; i < arraysize(test_cases); ++i) {
    const OriginCompare& test = test_cases[i];
    EXPECT_EQ(test.same_origin, HaveSameOrigin(test.a, test.b));
  }
}

TEST(UtilTests, IsValidUrlScheme) {
  struct Cases {
    const wchar_t* url;
    bool is_privileged;
    bool expected;
  } test_cases[] = {
    // non-privileged test cases
    { L"http://www.google.ca", false, true },
    { L"https://www.google.ca", false, true },
    { L"about:config", false, true },
    { L"view-source:http://www.google.ca", false, true },
    { L"chrome-extension://aaaaaaaaaaaaaaaaaaa/toolstrip.html", false, false },
    { L"ftp://www.google.ca", false, false },
    { L"file://www.google.ca", false, false },
    { L"file://C:\boot.ini", false, false },

    // privileged test cases
    { L"http://www.google.ca", true, true },
    { L"https://www.google.ca", true, true },
    { L"about:config", true, true },
    { L"view-source:http://www.google.ca", true, true },
    { L"chrome-extension://aaaaaaaaaaaaaaaaaaa/toolstrip.html", true, true },
    { L"ftp://www.google.ca", true, false },
    { L"file://www.google.ca", true, false },
    { L"file://C:\boot.ini", true, false },
  };

  for (int i = 0; i < arraysize(test_cases); ++i) {
    const Cases& test = test_cases[i];
    EXPECT_EQ(test.expected, IsValidUrlScheme(test.url, test.is_privileged));
  }
}

TEST(UtilTests, GuidToString) {
  // {3C5E2125-35BA-48df-A841-5F669B9D69FC}
  const GUID test_guid = { 0x3c5e2125, 0x35ba, 0x48df,
      { 0xa8, 0x41, 0x5f, 0x66, 0x9b, 0x9d, 0x69, 0xfc } };

  wchar_t compare[64] = {0};
  ::StringFromGUID2(test_guid, compare, arraysize(compare));

  std::wstring str_guid(GuidToString(test_guid));
  EXPECT_EQ(0, str_guid.compare(compare));
  EXPECT_EQ(static_cast<size_t>(lstrlenW(compare)), str_guid.length());
}

TEST(UtilTests, GetTempInternetFiles) {
  FilePath path = GetIETemporaryFilesFolder();
  EXPECT_FALSE(path.empty());
}

TEST(UtilTests, ParseAttachTabUrlTest) {
  ChromeFrameUrl cf_url;

  EXPECT_TRUE(cf_url.Parse(L"http://f/?attach_external_tab&10&1&0&0&100&100"));

  EXPECT_TRUE(cf_url.attach_to_external_tab());
  EXPECT_FALSE(cf_url.is_chrome_protocol());

  EXPECT_EQ(10, cf_url.cookie());
  EXPECT_EQ(1, cf_url.disposition());
  EXPECT_EQ(0, cf_url.dimensions().x());
  EXPECT_EQ(0, cf_url.dimensions().y());
  EXPECT_EQ(100, cf_url.dimensions().width());
  EXPECT_EQ(100, cf_url.dimensions().height());

  EXPECT_TRUE(cf_url.Parse(L"http://www.foobar.com?&10&1&0&0&100&100"));
  EXPECT_FALSE(cf_url.attach_to_external_tab());

  EXPECT_FALSE(cf_url.Parse(L"attach_external_tab&10&1"));
  EXPECT_TRUE(cf_url.attach_to_external_tab());

  EXPECT_TRUE(cf_url.Parse(L"gcf:http://f/?attach_tab&10&1&0&0&100&100"));
  EXPECT_FALSE(cf_url.attach_to_external_tab());
  EXPECT_TRUE(cf_url.is_chrome_protocol());
}

// Mock for the IInternetSecurityManager interface
class MockIInternetSecurityManager : public IInternetSecurityManager {
 public:
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, QueryInterface,
      HRESULT(REFIID iid, void** object));

  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, AddRef, ULONG());
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Release, ULONG());

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SetSecuritySite,
      HRESULT(IInternetSecurityMgrSite* site));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetSecuritySite,
      HRESULT(IInternetSecurityMgrSite** site));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, MapUrlToZone,
      HRESULT(LPCWSTR url, DWORD* zone, DWORD flags));
  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, GetSecurityId,
      HRESULT(LPCWSTR url, BYTE* security_id, DWORD* security_size,
              DWORD_PTR reserved));
  MOCK_METHOD8_WITH_CALLTYPE(__stdcall, ProcessUrlAction,
      HRESULT(LPCWSTR url, DWORD action, BYTE* policy, DWORD cb_policy,
              BYTE* context, DWORD context_size, DWORD flags,
              DWORD reserved));
  MOCK_METHOD7_WITH_CALLTYPE(__stdcall, QueryCustomPolicy,
      HRESULT(LPCWSTR url, REFGUID guid, BYTE** policy, DWORD* cb_policy,
              BYTE* context, DWORD cb_context, DWORD reserved));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, SetZoneMapping,
      HRESULT(DWORD zone, LPCWSTR pattern, DWORD flags));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, GetZoneMappings,
      HRESULT(DWORD zone, IEnumString** enum_string, DWORD flags));
};

TEST(UtilTests, CanNavigateFullTabModeTest) {
  ChromeFrameUrl cf_url;

  MockIInternetSecurityManager mock;
  EXPECT_CALL(mock, MapUrlToZone(testing::StartsWith(L"http://blah"),
                                                     testing::_, testing::_))
    .WillRepeatedly(testing::DoAll(
        testing::SetArgumentPointee<1>(URLZONE_INTERNET),
        testing::Return(S_OK)));

  EXPECT_CALL(mock, MapUrlToZone(testing::StartsWith(L"http://untrusted"),
                                                     testing::_, testing::_))
    .WillRepeatedly(testing::DoAll(
        testing::SetArgumentPointee<1>(URLZONE_UNTRUSTED),
        testing::Return(S_OK)));

  EXPECT_CALL(mock, MapUrlToZone(testing::StartsWith(L"about:"),
                                                     testing::_, testing::_))
    .WillRepeatedly(testing::DoAll(
        testing::SetArgumentPointee<1>(URLZONE_TRUSTED),
        testing::Return(S_OK)));

  EXPECT_CALL(mock, MapUrlToZone(testing::StartsWith(L"view-source:"),
                                                     testing::_, testing::_))
    .WillRepeatedly(testing::DoAll(
        testing::SetArgumentPointee<1>(URLZONE_TRUSTED),
        testing::Return(S_OK)));

  EXPECT_TRUE(cf_url.Parse(
      L"http://blah/?attach_external_tab&10&1&0&0&100&100"));
  EXPECT_TRUE(CanNavigateInFullTabMode(cf_url, &mock));

  EXPECT_TRUE(cf_url.Parse(
      L"http://untrusted/bar.html"));
  EXPECT_FALSE(CanNavigateInFullTabMode(cf_url, &mock));

  EXPECT_TRUE(cf_url.Parse(
      L"http://blah/?attach_external_tab&10&1&0&0&100&100"));
  EXPECT_TRUE(CanNavigateInFullTabMode(cf_url, &mock));

  EXPECT_TRUE(cf_url.Parse(L"gcf:about:blank"));
  EXPECT_TRUE(CanNavigateInFullTabMode(cf_url, &mock));

  EXPECT_TRUE(cf_url.Parse(L"gcf:view-source:http://www.foo.com"));
  EXPECT_TRUE(CanNavigateInFullTabMode(cf_url, &mock));

  EXPECT_TRUE(cf_url.Parse(L"gcf:about:version"));
  EXPECT_TRUE(CanNavigateInFullTabMode(cf_url, &mock));

  EXPECT_TRUE(cf_url.Parse(L"gcf:about:bar"));
  EXPECT_TRUE(CanNavigateInFullTabMode(cf_url, &mock));

  bool enable_gcf = GetConfigBool(false, kEnableGCFProtocol);

  SetConfigBool(kEnableGCFProtocol, false);

  EXPECT_TRUE(cf_url.Parse(L"gcf:http://blah"));
  EXPECT_FALSE(CanNavigateInFullTabMode(cf_url, &mock));

  SetConfigBool(kEnableGCFProtocol, true);

  EXPECT_TRUE(cf_url.Parse(L"gcf:http://blah"));
  EXPECT_TRUE(CanNavigateInFullTabMode(cf_url, &mock));

  EXPECT_TRUE(cf_url.Parse(L"gcf:http://untrusted/bar"));
  EXPECT_FALSE(CanNavigateInFullTabMode(cf_url, &mock));

  SetConfigBool(kEnableGCFProtocol, enable_gcf);
}

TEST(UtilTests, ParseVersionTest) {
  uint32 high = 0, low = 0;
  EXPECT_FALSE(ParseVersion(L"", &high, &low));
  EXPECT_TRUE(ParseVersion(L"1", &high, &low) && high == 1 && low == 0);
  EXPECT_TRUE(ParseVersion(L"1.", &high, &low) && high == 1 && low == 0);
  EXPECT_TRUE(ParseVersion(L"1.2", &high, &low) && high == 1 && low == 2);
  EXPECT_TRUE(ParseVersion(L"1.2.3.4", &high, &low) && high == 1 && low == 2);
  EXPECT_TRUE(ParseVersion(L"10.20", &high, &low) && high == 10 && low == 20);
}


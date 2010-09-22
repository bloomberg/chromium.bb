// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_version_info.h"
#include "base/file_version_info_win.h"
#include "base/registry.h"
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
    EXPECT_EQ(test.expected, IsValidUrlScheme(GURL(test.url),
                                              test.is_privileged));
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

  static const std::string kProfileName("iexplore");

  EXPECT_TRUE(cf_url.Parse(
      L"http://f/?attach_external_tab&10&1&2&3&123&321&iexplore"));

  EXPECT_TRUE(cf_url.attach_to_external_tab());
  EXPECT_FALSE(cf_url.is_chrome_protocol());
  EXPECT_EQ(10, cf_url.cookie());
  EXPECT_EQ(1, cf_url.disposition());
  EXPECT_EQ(gfx::Rect(2, 3, 123, 321), cf_url.dimensions());
  EXPECT_EQ(kProfileName, cf_url.profile_name());

  EXPECT_TRUE(cf_url.Parse(
      L"http://www.foobar.com?&10&1&2&3&123&321&iexplore"));
  EXPECT_FALSE(cf_url.attach_to_external_tab());
  EXPECT_FALSE(cf_url.is_chrome_protocol());
  EXPECT_EQ(0, cf_url.cookie());
  EXPECT_EQ(0, cf_url.disposition());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), cf_url.dimensions());
  EXPECT_TRUE(cf_url.profile_name().empty());

  EXPECT_FALSE(cf_url.Parse(L"attach_external_tab&10&1"));
  EXPECT_FALSE(cf_url.attach_to_external_tab());
  EXPECT_FALSE(cf_url.is_chrome_protocol());
  EXPECT_EQ(0, cf_url.cookie());
  EXPECT_EQ(0, cf_url.disposition());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), cf_url.dimensions());
  EXPECT_TRUE(cf_url.profile_name().empty());

  EXPECT_TRUE(cf_url.Parse(
      L"gcf:http://f/?attach_tab&10&1&2&3&123&321&iexplore"));
  EXPECT_FALSE(cf_url.attach_to_external_tab());
  EXPECT_TRUE(cf_url.is_chrome_protocol());
  EXPECT_EQ(0, cf_url.cookie());
  EXPECT_EQ(0, cf_url.disposition());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), cf_url.dimensions());
  EXPECT_TRUE(cf_url.profile_name().empty());

  EXPECT_TRUE(cf_url.Parse(L"gcf:http://google.com"));
  EXPECT_FALSE(cf_url.attach_to_external_tab());
  EXPECT_TRUE(cf_url.is_chrome_protocol());
  EXPECT_EQ(0, cf_url.cookie());
  EXPECT_EQ(0, cf_url.disposition());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), cf_url.dimensions());
  EXPECT_EQ(cf_url.gurl(), GURL("http://google.com"));
  EXPECT_TRUE(cf_url.profile_name().empty());
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

TEST(UtilTests, CanNavigateTest) {
  MockIInternetSecurityManager mock;

  struct Zones {
    const wchar_t* url_prefix;
    URLZONE zone;
  } test_zones[] = {
    { L"http://blah", URLZONE_INTERNET },
    { L"http://untrusted", URLZONE_UNTRUSTED },
    { L"about:", URLZONE_TRUSTED },
    { L"view-source:", URLZONE_TRUSTED },
    { L"chrome-extension:", URLZONE_TRUSTED },
    { L"ftp:", URLZONE_UNTRUSTED },
    { L"file:", URLZONE_LOCAL_MACHINE },
    { L"sip:", URLZONE_UNTRUSTED },
  };

  for (int i = 0; i < arraysize(test_zones); ++i) {
    const Zones& zone = test_zones[i];
    EXPECT_CALL(mock, MapUrlToZone(testing::StartsWith(zone.url_prefix),
                                                       testing::_, testing::_))
      .WillRepeatedly(testing::DoAll(
          testing::SetArgumentPointee<1>(zone.zone),
          testing::Return(S_OK)));
  }

  struct Cases {
    const char* url;
    bool is_privileged;
    bool default_expected;
    bool unsafe_expected;
  } test_cases[] = {
    // Invalid URL
    { "          ", false, false, false },
    { "foo bar", true, false, false },

    // non-privileged test cases
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore", false,
        true, true },
    { "http://untrusted/bar.html", false, false, true },
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore", false,
        true, true },
    { "view-source:http://www.google.ca", false, true, true },
    { "view-source:javascript:alert('foo');", false, false, true },
    { "about:blank", false, true, true },
    { "About:Version", false, true, true },
    { "about:config", false, false, true },
    { "chrome-extension://aaaaaaaaaaaaaaaaaaa/toolstrip.html", false, false,
        true },
    { "ftp://www.google.ca", false, false, true },
    { "file://www.google.ca", false, false, true },
    { "file://C:\boot.ini", false, false, true },
    { "SIP:someone@10.1.2.3", false, false, true },

    // privileged test cases
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore", true, true,
        true },
    { "http://untrusted/bar.html", true, false, true },
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore", true, true,
        true },
    { "view-source:http://www.google.ca", true, true, true },
    { "view-source:javascript:alert('foo');", true, false, true },
    { "about:blank", true, true, true },
    { "About:Version", true, true, true },
    { "about:config", true, false, true },
    { "chrome-extension://aaaaaaaaaaaaaaaaaaa/toolstrip.html", true, true,
        true },
    { "ftp://www.google.ca", true, false, true },
    { "file://www.google.ca", true, false, true },
    { "file://C:\boot.ini", true, false, true },
    { "sip:someone@10.1.2.3", false, false, true },
  };

  for (int i = 0; i < arraysize(test_cases); ++i) {
    const Cases& test = test_cases[i];
    bool actual = CanNavigate(GURL(test.url), &mock, test.is_privileged);
    EXPECT_EQ(test.default_expected, actual) << "Failure url: " << test.url;
  }

  bool enable_gcf = GetConfigBool(false, kAllowUnsafeURLs);
  SetConfigBool(kAllowUnsafeURLs, true);

  for (int i = 0; i < arraysize(test_cases); ++i) {
    const Cases& test = test_cases[i];
    bool actual = CanNavigate(GURL(test.url), &mock, test.is_privileged);
    EXPECT_EQ(test.unsafe_expected, actual) << "Failure url: " << test.url;
  }

  SetConfigBool(kAllowUnsafeURLs, enable_gcf);
}

TEST(UtilTests, IsDefaultRendererTest) {
  RegKey config_key(HKEY_CURRENT_USER, kChromeFrameConfigKey, KEY_ALL_ACCESS);
  EXPECT_TRUE(config_key.Valid());

  DWORD saved_default_renderer = 0;  // NOLINT
  config_key.ReadValueDW(kEnableGCFRendererByDefault, &saved_default_renderer);

  config_key.DeleteValue(kEnableGCFRendererByDefault);
  EXPECT_FALSE(IsGcfDefaultRenderer());

  config_key.WriteValue(kEnableGCFRendererByDefault, static_cast<DWORD>(0));
  EXPECT_FALSE(IsGcfDefaultRenderer());

  config_key.WriteValue(kEnableGCFRendererByDefault, static_cast<DWORD>(1));
  EXPECT_TRUE(IsGcfDefaultRenderer());

  config_key.WriteValue(kEnableGCFRendererByDefault, saved_default_renderer);
}

TEST(UtilTests, RendererTypeForUrlTest) {
  // Open all the keys we need.
  RegKey config_key(HKEY_CURRENT_USER, kChromeFrameConfigKey, KEY_ALL_ACCESS);
  EXPECT_TRUE(config_key.Valid());
  RegKey opt_for_gcf(config_key.Handle(), kRenderInGCFUrlList, KEY_ALL_ACCESS);
  EXPECT_TRUE(opt_for_gcf.Valid());
  RegKey opt_for_host(config_key.Handle(), kRenderInHostUrlList,
                      KEY_ALL_ACCESS);
  EXPECT_TRUE(opt_for_host.Valid());
  if (!config_key.Valid() || !opt_for_gcf.Valid() || !opt_for_host.Valid())
    return;

  const wchar_t kTestFilter[] = L"*.testing.chromium.org";
  const wchar_t kTestUrl[] = L"www.testing.chromium.org";

  // Save the current state of the registry.
  DWORD saved_default_renderer = 0;
  config_key.ReadValueDW(kEnableGCFRendererByDefault, &saved_default_renderer);

  // Make sure the host is the default renderer.
  config_key.WriteValue(kEnableGCFRendererByDefault, static_cast<DWORD>(0));
  EXPECT_FALSE(IsGcfDefaultRenderer());

  opt_for_gcf.DeleteValue(kTestFilter);  // Just in case this exists
  EXPECT_EQ(RENDERER_TYPE_UNDETERMINED, RendererTypeForUrl(kTestUrl));
  opt_for_gcf.WriteValue(kTestFilter, L"");
  EXPECT_EQ(RENDERER_TYPE_CHROME_OPT_IN_URL, RendererTypeForUrl(kTestUrl));

  // Now set GCF as the default renderer.
  config_key.WriteValue(kEnableGCFRendererByDefault, static_cast<DWORD>(1));
  EXPECT_TRUE(IsGcfDefaultRenderer());

  opt_for_host.DeleteValue(kTestFilter);  // Just in case this exists
  EXPECT_EQ(RENDERER_TYPE_CHROME_DEFAULT_RENDERER,
            RendererTypeForUrl(kTestUrl));
  opt_for_host.WriteValue(kTestFilter, L"");
  EXPECT_EQ(RENDERER_TYPE_UNDETERMINED, RendererTypeForUrl(kTestUrl));

  // Cleanup.
  opt_for_gcf.DeleteValue(kTestFilter);
  opt_for_host.DeleteValue(kTestFilter);
  config_key.WriteValue(kEnableGCFRendererByDefault, saved_default_renderer);
}


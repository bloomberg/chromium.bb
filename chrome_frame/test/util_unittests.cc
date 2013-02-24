// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"
#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome_frame/navigation_constraints.h"
#include "chrome_frame/registry_list_preferences_holder.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

const wchar_t kChannelName[] = L"-dev";
const wchar_t kSuffix[] = L"-fix";

// Registry override in the UtilsTest will cause shell APIs to fail
// So separate this test from the rest
TEST(SimpleUtilTests, GetTempInternetFiles) {
  base::FilePath path = GetIETemporaryFilesFolder();
  EXPECT_FALSE(path.empty());
}

class UtilTests : public testing::Test {
 protected:
  void SetUp() {
    DeleteAllSingletons();
  }

  void TearDown() {
    registry_virtualization_.RemoveAllOverrides();
  }

  // This is used to manage life cycle of PolicySettings singleton.
  // base::ShadowingAtExitManager at_exit_manager_;
  chrome_frame_test::ScopedVirtualizeHklmAndHkcu registry_virtualization_;
};

TEST_F(UtilTests, GetModuleVersionTest) {
  HMODULE mod = GetModuleHandle(L"kernel32.dll");
  EXPECT_NE(mod, static_cast<HMODULE>(NULL));
  wchar_t path[MAX_PATH] = {0};
  GetModuleFileName(mod, path, arraysize(path));

  // Use the method that goes to disk
  scoped_ptr<FileVersionInfo> base_info(
      FileVersionInfo::CreateFileVersionInfo(base::FilePath(path)));
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

TEST_F(UtilTests, HaveSameOrigin) {
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

TEST_F(UtilTests, IsValidUrlScheme) {
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
    { L"chrome-extension://aaaaaaaaaaaaaaaaaaa/monkey.html", false, false },
    { L"ftp://www.google.ca", false, false },
    { L"file://www.google.ca", false, false },
    { L"file://C:\boot.ini", false, false },

    // privileged test cases
    { L"http://www.google.ca", true, true },
    { L"https://www.google.ca", true, true },
    { L"about:config", true, true },
    { L"view-source:http://www.google.ca", true, true },
    { L"chrome-extension://aaaaaaaaaaaaaaaaaaa/monkey.html", true, true },
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

TEST_F(UtilTests, GuidToString) {
  // {3C5E2125-35BA-48df-A841-5F669B9D69FC}
  const GUID test_guid = { 0x3c5e2125, 0x35ba, 0x48df,
      { 0xa8, 0x41, 0x5f, 0x66, 0x9b, 0x9d, 0x69, 0xfc } };

  wchar_t compare[64] = {0};
  ::StringFromGUID2(test_guid, compare, arraysize(compare));

  std::wstring str_guid(GuidToString(test_guid));
  EXPECT_EQ(0, str_guid.compare(compare));
  EXPECT_EQ(static_cast<size_t>(lstrlenW(compare)), str_guid.length());
}

TEST_F(UtilTests, ParseAttachTabUrlTest) {
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

// This class provides a partial mock for the NavigationConstraints
// interface by providing specialized zone overrides.
class MockNavigationConstraintsZoneOverride
    : public NavigationConstraintsImpl {
 public:
  MOCK_METHOD1(IsZoneAllowed, bool(const GURL&url));
};

// Mock NavigationConstraints
class MockNavigationConstraints : public NavigationConstraints {
 public:
  MOCK_METHOD0(AllowUnsafeUrls, bool());
  MOCK_METHOD1(IsSchemeAllowed, bool(const GURL& url));
  MOCK_METHOD1(IsZoneAllowed, bool(const GURL& url));
};

// Matcher which returns true if the URL passed in starts with the prefix
// specified.
MATCHER_P(UrlPathStartsWith, url_prefix, "url starts with prefix") {
  return StartsWith(UTF8ToWide(arg.spec()), url_prefix, false);
}

ACTION_P3(HandleZone, mock, url_prefix, zone) {
  if (StartsWith(UTF8ToWide(arg0.spec()), url_prefix, false))
    return zone != URLZONE_UNTRUSTED;
  return false;
}

TEST_F(UtilTests, CanNavigateTest) {
  MockNavigationConstraintsZoneOverride mock;

  struct Zones {
    const wchar_t* url_prefix;
    URLZONE zone;
  } test_zones[] = {
    { L"http://blah", URLZONE_INTERNET },
    { L"http://untrusted", URLZONE_UNTRUSTED },
    { L"about:", URLZONE_TRUSTED },
    { L"view-source:", URLZONE_TRUSTED },
    { L"chrome-extension:", URLZONE_TRUSTED },
    { L"data:", URLZONE_INTERNET },
    { L"ftp:", URLZONE_UNTRUSTED },
    { L"file:", URLZONE_LOCAL_MACHINE },
    { L"sip:", URLZONE_UNTRUSTED },
  };

  for (int i = 0; i < arraysize(test_zones); ++i) {
    const Zones& zone = test_zones[i];
    EXPECT_CALL(mock, IsZoneAllowed(UrlPathStartsWith(zone.url_prefix)))
        .WillRepeatedly(testing::Return(zone.zone != URLZONE_UNTRUSTED));
  }

  struct Cases {
    const char* url;
    bool default_expected;
    bool unsafe_expected;
    bool is_privileged;
  } test_cases[] = {
    // Invalid URL
    { "          ", false, false, false },
    { "foo bar", false, false, false },

    // non-privileged test cases
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore", true,
       true, false },
    { "http://untrusted/bar.html", false, true, false },
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore", true,
       true, false },
    { "view-source:http://www.google.ca", true, true, false },
    { "view-source:javascript:alert('foo');", false, true, false },
    { "about:blank", true, true, false },
    { "About:Version", true, true, false },
    { "about:config", false, true, false },
    { "chrome-extension://aaaaaaaaaaaaaaaaaaa/monkey.html", false, true,
       false },
    { "ftp://www.google.ca", false, true, false },
    { "file://www.google.ca", false, true, false },
    { "file://C:\boot.ini", false, true, false },
    { "SIP:someone@10.1.2.3", false, true, false },

    // privileged test cases
    { "chrome-extension://aaaaaaaaaaaaaaaaaaa/monkey.html", true, true,
       true },
    { "data://aaaaaaaaaaaaaaaaaaa/monkey.html", true, true, true },
  };

  for (int i = 0; i < arraysize(test_cases); ++i) {
    const Cases& test = test_cases[i];
    mock.set_is_privileged(test.is_privileged);
    bool actual = CanNavigate(GURL(test.url), &mock);
    EXPECT_EQ(test.default_expected, actual) << "Failure url: " << test.url;
  }
}

TEST_F(UtilTests, CanNavigateTestDenyAll) {
  MockNavigationConstraints mock;

  EXPECT_CALL(mock, IsZoneAllowed(testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(mock, IsSchemeAllowed(testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(mock, AllowUnsafeUrls())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(false));

  char *urls[] = {
    { "          "},
    { "foo bar"},
    // non-privileged test cases
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore"},
    { "http://untrusted/bar.html"},
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore"},
    { "view-source:http://www.google.ca"},
    { "view-source:javascript:alert('foo');"},
    { "about:blank"},
    { "About:Version"},
    { "about:config"},
    { "chrome-extension://aaaaaaaaaaaaaaaaaaa/monkey.html"},
    { "ftp://www.google.ca"},
    { "file://www.google.ca"},
    { "file://C:\boot.ini"},
    { "SIP:someone@10.1.2.3"},
  };

  for (int i = 0; i < arraysize(urls); ++i) {
    EXPECT_FALSE(CanNavigate(GURL(urls[i]), &mock));
  }
}

TEST_F(UtilTests, CanNavigateTestAllowAll) {
  MockNavigationConstraints mock;

  EXPECT_CALL(mock, AllowUnsafeUrls())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(mock, IsSchemeAllowed(testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(mock, IsZoneAllowed(testing::_))
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(true));

  char *urls[] = {
    // non-privileged test cases
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore"},
    { "http://untrusted/bar.html"},
    { "http://blah/?attach_external_tab&10&1&0&0&100&100&iexplore"},
    { "view-source:http://www.google.ca"},
    { "view-source:javascript:alert('foo');"},
    { "about:blank"},
    { "About:Version"},
    { "about:config"},
    { "chrome-extension://aaaaaaaaaaaaaaaaaaa/monkey.html"},
    { "ftp://www.google.ca"},
    { "file://www.google.ca"},
    { "file://C:\boot.ini"},
    { "SIP:someone@10.1.2.3"},
    { "gcf:about:cache"},
    { "gcf:about:plugins"},
  };

  for (int i = 0; i < arraysize(urls); ++i) {
    EXPECT_TRUE(CanNavigate(GURL(urls[i]), &mock));
  }
}

TEST_F(UtilTests, CanNavigateTestAllowAllUnsafeUrls) {
  MockNavigationConstraints mock;

  EXPECT_CALL(mock, AllowUnsafeUrls())
      .Times(testing::AnyNumber())
      .WillRepeatedly(testing::Return(true));

  char *urls[] = {
    {"gcf:about:cache"},
    {"gcf:http://www.google.com"},
    {"view-source:javascript:alert('foo');"},
    {"http://www.google.com"},
  };

  for (int i = 0; i < arraysize(urls); ++i) {
    EXPECT_TRUE(CanNavigate(GURL(urls[i]), &mock));
  }
}

TEST_F(UtilTests, IsDefaultRendererTest) {
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

TEST_F(UtilTests, RendererTypeForUrlTest) {
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

  // We need to manually reset the holder between checks.
  // TODO(robertshield): Remove this when the RegistryWatcher is wired up.
  RegistryListPreferencesHolder& renderer_type_preferences_holder =
      GetRendererTypePreferencesHolderForTesting();

  // Make sure the host is the default renderer.
  config_key.WriteValue(kEnableGCFRendererByDefault, static_cast<DWORD>(0));
  EXPECT_FALSE(IsGcfDefaultRenderer());

  opt_for_gcf.DeleteValue(kTestFilter);  // Just in case this exists
  EXPECT_EQ(RENDERER_TYPE_UNDETERMINED, RendererTypeForUrl(kTestUrl));
  opt_for_gcf.WriteValue(kTestFilter, L"");
  renderer_type_preferences_holder.ResetForTesting();
  EXPECT_EQ(RENDERER_TYPE_CHROME_OPT_IN_URL, RendererTypeForUrl(kTestUrl));

  // Now set GCF as the default renderer.
  config_key.WriteValue(kEnableGCFRendererByDefault, static_cast<DWORD>(1));
  EXPECT_TRUE(IsGcfDefaultRenderer());

  opt_for_host.DeleteValue(kTestFilter);  // Just in case this exists
  renderer_type_preferences_holder.ResetForTesting();
  EXPECT_EQ(RENDERER_TYPE_CHROME_DEFAULT_RENDERER,
            RendererTypeForUrl(kTestUrl));
  opt_for_host.WriteValue(kTestFilter, L"");
  renderer_type_preferences_holder.ResetForTesting();
  EXPECT_EQ(RENDERER_TYPE_UNDETERMINED, RendererTypeForUrl(kTestUrl));

  // Cleanup.
  opt_for_gcf.DeleteValue(kTestFilter);
  opt_for_host.DeleteValue(kTestFilter);
  config_key.WriteValue(kEnableGCFRendererByDefault, saved_default_renderer);
  renderer_type_preferences_holder.ResetForTesting();
  RendererTypeForUrl(L"");
}

TEST_F(UtilTests, XUaCompatibleDirectiveTest) {
  int all_versions[] = {0, 1, 2, 5, 6, 7, 8, 9, 10, 11, 99, 100, 101, 1000};

  struct Cases {
    const char* header_value;
    int max_version;
  } test_cases[] = {
    // Negative cases
    { "", -1 },
    { "chrome=", -1 },
    { "chrome", -1 },
    { "chrome=X", -1 },
    { "chrome=IE", -1 },
    { "chrome=IE-7", -1 },
    { "chrome=IE+7", -1 },
    { "chrome=IE 7", -1 },
    { "chrome=IE7.0", -1 },
    { "chrome=FF7", -1 },
    { "chrome=IE7+", -1 },
    { "chrome=IE99999999999999999999", -1 },
    { "chrome=IE0", -1 },
    // Always on
    { "chrome=1", INT_MAX },
    // Basic positive cases
    { "chrome=IE1", 1 },
    { "CHROME=IE6", 6 },
    { "Chrome=IE10", 10 },
    { "ChRoMe=IE100", 100 },
    // Positive formatting variations
    { "  chrome=IE6  ", 6 },
    { "  chrome=IE6;  ", 6 },
    { "  chrome=IE6; IE=8 ", 6 },
    { "  IE=8;chrome=IE6;", 6 },
    { "  IE=8;chrome=IE6;", 6 },
    { "  IE=8 ; chrome = IE6 ;", 6 },
    // Ignore unrecognized values
    { "  IE=8 ; chrome = IE7.1; chrome = IE6;", 6 },
    // First valid wins
    { "  IE=8 ; chrome = IE6; chrome = IE8;", 6 },
    // Comma delimiter
    { "  IE=8,chrome=IE6;", -1 },
    { "  IE=8,chrome=IE6", 6 },
    { "  IE=8,chrome=IE6, Something=Else;Why;Not", 6 },
    { "  IE=8,chrome=1,Something=Else", INT_MAX },
    { "  IE=8(a;b;c),chrome=IE7,Something=Else", 7 }
  };

  for (int case_index = 0; case_index < arraysize(test_cases); ++case_index) {
    const Cases& test = test_cases[case_index];

    // Check that all versions <= max_version are matched
    for (size_t version_index = 0;
         version_index < arraysize(all_versions);
         ++version_index) {
      bool expect_match = (all_versions[version_index] <= test.max_version);

      ASSERT_EQ(expect_match,
                CheckXUaCompatibleDirective(test.header_value,
                                            all_versions[version_index]))
          << "Expect '" << test.header_value << "' to "
          << (expect_match ? "match" : "not match") << " IE major version "
          << all_versions[version_index];
    }
  }
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"
#include "base/file_version_info_win.h"
#include "chrome_frame/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  std::wstring url = L"attach_external_tab&10&1&0&0&100&100";

  uint64 cookie = 0;
  gfx::Rect dimensions;
  int disposition = 0;

  EXPECT_TRUE(ParseAttachExternalTabUrl(url, &cookie, &dimensions,
                                        &disposition));
  EXPECT_EQ(10, cookie);
  EXPECT_EQ(1, disposition);
  EXPECT_EQ(0, dimensions.x());
  EXPECT_EQ(0, dimensions.y());
  EXPECT_EQ(100, dimensions.width());
  EXPECT_EQ(100, dimensions.height());

  url = L"http://www.foobar.com?&10&1&0&0&100&100";
  EXPECT_FALSE(ParseAttachExternalTabUrl(url, &cookie, &dimensions,
                                         &disposition));
  url = L"attach_external_tab&10&1";
  EXPECT_FALSE(ParseAttachExternalTabUrl(url, &cookie, &dimensions,
                                         &disposition));
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


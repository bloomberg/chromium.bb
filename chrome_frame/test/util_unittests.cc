// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"
#include "chrome_frame/test/chrome_frame_unittests.h"
#include "chrome_frame/utils.h"

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
  EXPECT_NE(high, 0);
  EXPECT_NE(low, 0);

  // Make sure they give the same results.
  VS_FIXEDFILEINFO* fixed_info = base_info->fixed_file_info();
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

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for BrowserDistribution class.

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/shell_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class BrowserDistributionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Currently no setup required.
  }

  virtual void TearDown() {
    // Currently no tear down required.
  }
};
}  // namespace

// The distribution strings should not be empty. The unit tests are not linking
// with the chrome resources so we cannot test official build.
TEST(BrowserDistributionTest, StringsTest) {
  struct browser_test_type {
    BrowserDistribution::Type type;
    bool has_com_dlls;
  } browser_tests[] = {
    { BrowserDistribution::CHROME_BROWSER, false },
    { BrowserDistribution::CHROME_FRAME, true },
  };

  const installer::MasterPreferences& prefs =
      installer::MasterPreferences::ForCurrentProcess();

  for (int i = 0; i < arraysize(browser_tests); ++i) {
    BrowserDistribution* dist =
        BrowserDistribution::GetSpecificDistribution(browser_tests[i].type,
                                                     prefs);
    ASSERT_TRUE(dist != NULL);
    std::wstring name = dist->GetApplicationName();
    EXPECT_FALSE(name.empty());
    std::wstring desc = dist->GetAppDescription();
    EXPECT_FALSE(desc.empty());
    std::wstring alt_name = dist->GetAlternateApplicationName();
    EXPECT_FALSE(alt_name.empty());
    std::vector<FilePath> com_dlls(dist->GetComDllList());
    if (browser_tests[i].has_com_dlls) {
      EXPECT_FALSE(com_dlls.empty());
    } else {
      EXPECT_TRUE(com_dlls.empty());
    }
  }
}

// The shortcut strings obtained by the shell utility functions should not
// be empty or be the same.
TEST(BrowserDistributionTest, AlternateAndNormalShortcutName) {
  std::wstring normal_name;
  std::wstring alternate_name;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  EXPECT_TRUE(ShellUtil::GetChromeShortcutName(dist, &normal_name, false));
  EXPECT_TRUE(ShellUtil::GetChromeShortcutName(dist, &alternate_name, true));
  EXPECT_NE(normal_name, alternate_name);
  EXPECT_FALSE(normal_name.empty());
  EXPECT_FALSE(alternate_name.empty());
}

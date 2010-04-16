// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/registry.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const wchar_t* kHKCUReplacement =
    L"Software\\Google\\InstallUtilUnittest\\HKCU";
const wchar_t* kHKLMReplacement =
    L"Software\\Google\\InstallUtilUnittest\\HKLM";

// This test fixture redirects the HKLM and HKCU registry hives for
// the duration of the test to make it independent of the machine
// and user settings.
class GoogleUpdateSettingsTest: public testing::Test {
 protected:
  virtual void SetUp() {
    // Wipe the keys we redirect to.
    // This gives us a stable run, even in the presence of previous
    // crashes or failures.
    LSTATUS err = SHDeleteKey(HKEY_CURRENT_USER, kHKCUReplacement);
    EXPECT_TRUE(err == ERROR_SUCCESS || err == ERROR_FILE_NOT_FOUND);
    err = SHDeleteKey(HKEY_CURRENT_USER, kHKLMReplacement);
    EXPECT_TRUE(err == ERROR_SUCCESS || err == ERROR_FILE_NOT_FOUND);

    // Create the keys we're redirecting HKCU and HKLM to.
    ASSERT_TRUE(hkcu_.Create(HKEY_CURRENT_USER, kHKCUReplacement));
    ASSERT_TRUE(hklm_.Create(HKEY_CURRENT_USER, kHKLMReplacement));

    // And do the switcharoo.
    ASSERT_EQ(ERROR_SUCCESS,
              ::RegOverridePredefKey(HKEY_CURRENT_USER, hkcu_.Handle()));
    ASSERT_EQ(ERROR_SUCCESS,
              ::RegOverridePredefKey(HKEY_LOCAL_MACHINE, hklm_.Handle()));
  }

  virtual void TearDown() {
    // Undo the redirection.
    EXPECT_EQ(ERROR_SUCCESS, ::RegOverridePredefKey(HKEY_CURRENT_USER, NULL));
    EXPECT_EQ(ERROR_SUCCESS, ::RegOverridePredefKey(HKEY_LOCAL_MACHINE, NULL));

    // Close our handles and delete the temp keys we redirected to.
    hkcu_.Close();
    hklm_.Close();
    EXPECT_EQ(ERROR_SUCCESS, SHDeleteKey(HKEY_CURRENT_USER, kHKCUReplacement));
    EXPECT_EQ(ERROR_SUCCESS, SHDeleteKey(HKEY_CURRENT_USER, kHKLMReplacement));
  }

  enum SystemUserInstall {
    SYSTEM_INSTALL,
    USER_INSTALL,
  };

  void SetApField(SystemUserInstall is_system, const wchar_t* value) {
    HKEY root = is_system == SYSTEM_INSTALL ?
        HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    RegKey update_key;
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    std::wstring path = dist->GetStateKey();
    ASSERT_TRUE(update_key.Create(root, path.c_str(), KEY_WRITE));
    ASSERT_TRUE(update_key.WriteValue(L"ap", value));
  }

  // Tests setting the ap= value to various combinations of values with
  // prefixes and suffixes, while asserting on the correct channel value.
  // Note that any non-empty ap= value that doesn't match ".*-{dev|beta}.*"
  // will return the "unknown" channel.
  void TestCurrentChromeChannelWithVariousApValues(SystemUserInstall install) {
    static struct Expectations {
      const wchar_t* ap_value;
      const wchar_t* channel;
    } expectations[] = {
      { L"dev", L"unknown" },
      { L"-dev", L"dev" },
      { L"-developer", L"dev" },
      { L"beta", L"unknown" },
      { L"-beta", L"beta" },
      { L"-betamax", L"beta" },
    };
    bool is_system = install == SYSTEM_INSTALL;
    const wchar_t* prefixes[] = {
      L"",
      L"prefix",
      L"prefix-with-dash",
    };
    const wchar_t* suffixes[] = {
      L"",
      L"suffix",
      L"suffix-with-dash",
    };

    for (size_t i = 0; i < arraysize(prefixes); ++i) {
      for (size_t j = 0; j < arraysize(expectations); ++j) {
        for (size_t k = 0; k < arraysize(suffixes); ++k) {
          std::wstring ap = prefixes[i];
          ap += expectations[j].ap_value;
          ap += suffixes[k];
          const wchar_t* channel = expectations[j].channel;

          SetApField(install, ap.c_str());
          std::wstring ret_channel;

          EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannel(&ret_channel));
          EXPECT_STREQ(channel, ret_channel.c_str())
              << "Expecting channel \"" << channel
              << "\" for ap=\"" << ap << "\"";
        }
      }
    }
  }

  RegKey hkcu_;
  RegKey hklm_;
};

}  // namespace

// Verify that we return failure on no registration.
TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelAbsent) {
  std::wstring channel;
  EXPECT_FALSE(GoogleUpdateSettings::GetChromeChannel(&channel));
  EXPECT_STREQ(L"unknown", channel.c_str());
}

// Test an empty Ap key for system and user.
TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelEmptySystem) {
  SetApField(SYSTEM_INSTALL, L"");
  std::wstring channel;
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannel(&channel));
  EXPECT_STREQ(L"", channel.c_str());
}

TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelEmptyUser) {
  SetApField(USER_INSTALL, L"");
  std::wstring channel;
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannel(&channel));
  EXPECT_STREQ(L"", channel.c_str());
}

TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelVariousApValuesSystem) {
  TestCurrentChromeChannelWithVariousApValues(SYSTEM_INSTALL);
}

TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelVariousApValuesUser) {
  TestCurrentChromeChannelWithVariousApValues(USER_INSTALL);
}

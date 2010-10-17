// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h> // For SHDeleteKey.

#include "base/scoped_ptr.h"
#include "base/win/registry.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

const wchar_t kHKCUReplacement[] =
    L"Software\\Google\\InstallUtilUnittest\\HKCU";
const wchar_t kHKLMReplacement[] =
    L"Software\\Google\\InstallUtilUnittest\\HKLM";

const wchar_t kTestProductGuid[] = L"{89F1B351-B15D-48D4-8F10-1298721CF13D}";

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
    ASSERT_TRUE(hkcu_.Create(HKEY_CURRENT_USER, kHKCUReplacement, KEY_READ));
    ASSERT_TRUE(hklm_.Create(HKEY_CURRENT_USER, kHKLMReplacement, KEY_READ));

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

          EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannel(is_system,
              &ret_channel));
          EXPECT_STREQ(channel, ret_channel.c_str())
              << "Expecting channel \"" << channel
              << "\" for ap=\"" << ap << "\"";
        }
      }
    }
  }

  // Creates "ap" key with the value given as parameter. Also adds work
  // items to work_item_list given so that they can be rolled back later.
  bool CreateApKey(WorkItemList* work_item_list, const std::wstring& value) {
    HKEY reg_root = HKEY_CURRENT_USER;
    std::wstring reg_key = GetApKeyPath();
    work_item_list->AddCreateRegKeyWorkItem(reg_root, reg_key);
    work_item_list->AddSetRegValueWorkItem(reg_root, reg_key,
        google_update::kRegApField, value.c_str(), true);
    if (!work_item_list->Do()) {
      work_item_list->Rollback();
      return false;
    }
    return true;
  }

  // Returns the key path of "ap" key, e.g.:
  // Google\Update\ClientState\<kTestProductGuid>
  std::wstring GetApKeyPath() {
    std::wstring reg_key(google_update::kRegPathClientState);
    reg_key.append(L"\\");
    reg_key.append(kTestProductGuid);
    return reg_key;
  }

  // Utility method to read "ap" key value
  std::wstring ReadApKeyValue() {
    RegKey key;
    std::wstring ap_key_value;
    std::wstring reg_key = GetApKeyPath();
    if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS) &&
        key.ReadValue(google_update::kRegApField, &ap_key_value)) {
      return ap_key_value;
    }
    return std::wstring();
  }

  RegKey hkcu_;
  RegKey hklm_;
};

}  // namespace

// Verify that we return failure on no registration,
// whether per-system or per-user install.
TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelAbsent) {
  // Per-system first.
  std::wstring channel;
  EXPECT_FALSE(GoogleUpdateSettings::GetChromeChannel(true, &channel));
  EXPECT_STREQ(L"unknown", channel.c_str());

  // Then per-user.
  EXPECT_FALSE(GoogleUpdateSettings::GetChromeChannel(false, &channel));
  EXPECT_STREQ(L"unknown", channel.c_str());
}

// Test an empty Ap key for system and user.
TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelEmptySystem) {
  SetApField(SYSTEM_INSTALL, L"");
  std::wstring channel;
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannel(true, &channel));
  EXPECT_STREQ(L"", channel.c_str());

  // Per-user lookups should fail.
  EXPECT_FALSE(GoogleUpdateSettings::GetChromeChannel(false, &channel));
}

TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelEmptyUser) {
  SetApField(USER_INSTALL, L"");
  // Per-system lookup should fail.
  std::wstring channel;
  EXPECT_FALSE(GoogleUpdateSettings::GetChromeChannel(true, &channel));

  // Per-user lookup should succeed.
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannel(false, &channel));
  EXPECT_STREQ(L"", channel.c_str());
}

TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelVariousApValuesSystem) {
  TestCurrentChromeChannelWithVariousApValues(SYSTEM_INSTALL);
}

TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelVariousApValuesUser) {
  TestCurrentChromeChannelWithVariousApValues(USER_INSTALL);
}

TEST_F(GoogleUpdateSettingsTest, GetNewGoogleUpdateApKeyTest) {
  installer_util::InstallStatus s = installer_util::FIRST_INSTALL_SUCCESS;
  installer_util::InstallStatus f = installer_util::INSTALL_FAILED;

  // Incremental Installer that worked.
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, s, L""), L"");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, s, L"1.1"),
            L"1.1");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, s, L"1.1-dev"),
            L"1.1-dev");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, s, L"-full"),
            L"");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, s, L"1.1-full"),
            L"1.1");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, s,
                                                          L"1.1-dev-full"),
            L"1.1-dev");

  // Incremental Installer that failed.
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, f, L""),
            L"-full");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, f, L"1.1"),
            L"1.1-full");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, f, L"1.1-dev"),
            L"1.1-dev-full");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, f, L"-full"),
            L"-full");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, f, L"1.1-full"),
            L"1.1-full");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(true, f,
                                                          L"1.1-dev-full"),
            L"1.1-dev-full");

  // Full Installer that worked.
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, s, L""), L"");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, s, L"1.1"),
            L"1.1");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, s, L"1.1-dev"),
            L"1.1-dev");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, s, L"-full"),
            L"");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, s,
                                                          L"1.1-full"),
            L"1.1");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, s,
                                                          L"1.1-dev-full"),
            L"1.1-dev");

  // Full Installer that failed.
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, f, L""), L"");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, f, L"1.1"),
            L"1.1");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, f, L"1.1-dev"),
            L"1.1-dev");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, f, L"-full"),
            L"");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, f,
                                                          L"1.1-full"),
            L"1.1");
  EXPECT_EQ(GoogleUpdateSettings::GetNewGoogleUpdateApKey(false, f,
                                                          L"1.1-dev-full"),
            L"1.1-dev");
}

TEST_F(GoogleUpdateSettingsTest, UpdateDiffInstallStatusTest) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  // Test incremental install failure
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L""))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateDiffInstallStatus(false, true,
                                       installer_util::INSTALL_FAILED,
                                       kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test incremental install success
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L""))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateDiffInstallStatus(false, true,
                                       installer_util::FIRST_INSTALL_SUCCESS,
                                       kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install failure
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L"-full"))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateDiffInstallStatus(false, false,
                                       installer_util::INSTALL_FAILED,
                                       kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install success
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L"-full"))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateDiffInstallStatus(false, false,
                                       installer_util::FIRST_INSTALL_SUCCESS,
                                       kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test the case of when "ap" key doesnt exist at all
  std::wstring ap_key_value = ReadApKeyValue();
  std::wstring reg_key = GetApKeyPath();
  HKEY reg_root = HKEY_CURRENT_USER;
  bool ap_key_deleted = false;
  RegKey key;
  if (!key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS)) {
    work_item_list->AddCreateRegKeyWorkItem(reg_root, reg_key);
    ASSERT_TRUE(work_item_list->Do()) << "Failed to create ClientState key.";
  } else if (key.DeleteValue(google_update::kRegApField)) {
    ap_key_deleted = true;
  }
  // try differential installer
  GoogleUpdateSettings::UpdateDiffInstallStatus(false, true,
                                       installer_util::INSTALL_FAILED,
                                       kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  // try full installer now
  GoogleUpdateSettings::UpdateDiffInstallStatus(false, false,
                                       installer_util::INSTALL_FAILED,
                                       kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  // Now cleanup to leave the system in unchanged state.
  // - Diff installer creates an ap key if it didnt exist, so delete this ap key
  // - If we created any reg key path for ap, roll it back
  // - Finally restore the original value of ap key.
  key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS);
  key.DeleteValue(google_update::kRegApField);
  work_item_list->Rollback();
  if (ap_key_deleted) {
    work_item_list.reset(WorkItem::CreateWorkItemList());
    ASSERT_TRUE(CreateApKey(work_item_list.get(), ap_key_value))
        << "Failed to restore ap key.";
  }
}

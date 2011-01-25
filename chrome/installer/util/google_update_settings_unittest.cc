// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>  // For SHDeleteKey.

#include <functional>
#include <map>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/win/registry.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using installer::ChannelInfo;

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
    ASSERT_EQ(ERROR_SUCCESS,
        hkcu_.Create(HKEY_CURRENT_USER, kHKCUReplacement, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS,
        hklm_.Create(HKEY_CURRENT_USER, kHKLMReplacement, KEY_READ));

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
    ASSERT_EQ(ERROR_SUCCESS, update_key.Create(root, path.c_str(), KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS, update_key.WriteValue(L"ap", value));
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
      { L"dev", L"dev" },
      { L"-dev", L"dev" },
      { L"-developer", L"dev" },
      { L"beta", L"beta" },
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
    if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS) ==
        ERROR_SUCCESS) {
      key.ReadValue(google_update::kRegApField, &ap_key_value);
    }

    return ap_key_value;
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

// Run through all combinations of diff vs. full install, single vs. multi
// install, success and failure results, and a fistfull of initial "ap" values
// checking that the expected final "ap" value is generated by
// GoogleUpdateSettings::UpdateGoogleUpdateApKey.
TEST_F(GoogleUpdateSettingsTest, UpdateGoogleUpdateApKey) {
  const bool is_multi[] = {
    false,
    true
  };
  const bool is_diff[] = {
    false,
    true
  };
  const int results[] = {
    installer::FIRST_INSTALL_SUCCESS,
    installer::INSTALL_FAILED
  };
  const wchar_t* const plain[] = {
    L"",
    L"1.1",
    L"1.1-dev"
  };
  const wchar_t* const full[] = {
    L"-full",
    L"1.1-full",
    L"1.1-dev-full"
  };
  COMPILE_ASSERT(arraysize(full) == arraysize(plain), bad_full_array_size);
  const wchar_t* const multifail[] = {
    L"-multifail",
    L"1.1-multifail",
    L"1.1-dev-multifail"
  };
  COMPILE_ASSERT(arraysize(multifail) == arraysize(plain),
                 bad_multifail_array_size);
  const wchar_t* const multifail_full[] = {
    L"-multifail-full",
    L"1.1-multifail-full",
    L"1.1-dev-multifail-full"
  };
  COMPILE_ASSERT(arraysize(multifail_full) == arraysize(plain),
                 bad_multifail_full_array_size);
  const wchar_t* const* input_arrays[] = {
    plain,
    full,
    multifail,
    multifail_full
  };
  ChannelInfo v;
  for (int multi_idx = 0; multi_idx < arraysize(is_multi); ++multi_idx) {
    const bool multi = is_multi[multi_idx];
    for (int diff_idx = 0; diff_idx < arraysize(is_diff); ++diff_idx) {
      const bool diff = is_diff[diff_idx];
      for (int result_idx = 0; result_idx < arraysize(results); ++result_idx) {
        const int result = results[result_idx];
        const wchar_t* const* outputs;
        if (result == installer::FIRST_INSTALL_SUCCESS || !multi && !diff)
          outputs = plain;
        else if (!multi && diff)
          outputs = full;
        else if (multi && !diff)
          outputs = multifail;
        else
          outputs = multifail_full;

        for (int inputs_idx = 0; inputs_idx < arraysize(input_arrays);
             ++inputs_idx) {
          const wchar_t* const* inputs = input_arrays[inputs_idx];
          for (int input_idx = 0; input_idx < arraysize(plain); ++input_idx) {
            const wchar_t* input = inputs[input_idx];
            const wchar_t* output = outputs[input_idx];

            v.set_value(input);
            if (output == v.value()) {
              EXPECT_FALSE(GoogleUpdateSettings::UpdateGoogleUpdateApKey(
                  diff, multi, result, &v))
                  << "diff: " << diff
                  << ", multi: " << multi
                  << ", result: " << result
                  << ", input ap value: " << input;
            } else {
              EXPECT_TRUE(GoogleUpdateSettings::UpdateGoogleUpdateApKey(
                  diff, multi, result, &v))
                  << "diff: " << diff
                  << ", multi: " << multi
                  << ", result: " << result
                  << ", input ap value: " << input;
            }
            EXPECT_EQ(output, v.value())
                << "diff: " << diff
                << ", multi: " << multi
                << ", result: " << result
                << ", input ap value: " << input;
          }
        }
      }
    }
  }
}

TEST_F(GoogleUpdateSettingsTest, UpdateInstallStatusTest) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  // Test incremental install failure
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L""))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false, true, false,
                                            installer::INSTALL_FAILED,
                                            kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test incremental install success
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L""))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false, true, false,
                                            installer::FIRST_INSTALL_SUCCESS,
                                            kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install failure
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L"-full"))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false, false, false,
                                            installer::INSTALL_FAILED,
                                            kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install success
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L"-full"))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false, false, false,
                                            installer::FIRST_INSTALL_SUCCESS,
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
  if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS) !=
      ERROR_SUCCESS) {
    work_item_list->AddCreateRegKeyWorkItem(reg_root, reg_key);
    ASSERT_TRUE(work_item_list->Do()) << "Failed to create ClientState key.";
  } else if (key.DeleteValue(google_update::kRegApField) == ERROR_SUCCESS) {
    ap_key_deleted = true;
  }
  // try differential installer
  GoogleUpdateSettings::UpdateInstallStatus(false, true, false,
                                            installer::INSTALL_FAILED,
                                            kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  // try full installer now
  GoogleUpdateSettings::UpdateInstallStatus(false, false, false,
                                            installer::INSTALL_FAILED,
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

TEST_F(GoogleUpdateSettingsTest, SetEULAConsent) {
  const bool multi_install = true;
  const bool system_level = true;
  CommandLine cmd_line = CommandLine::FromString(
      std::wstring(L"setup.exe") +
      (multi_install ? L" --multi-install --chrome" : L"") +
      (system_level ? L" --system-level" : L""));
  installer::MasterPreferences prefs(cmd_line);
  installer::InstallationState machine_state;
  machine_state.Initialize();
  installer::InstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, machine_state);
  HKEY root = installer_state.root_key();

  RegKey key;
  DWORD value;
  BrowserDistribution* binaries =
      installer_state.multi_package_binaries_distribution();
  EXPECT_EQ(BrowserDistribution::CHROME_BINARIES, binaries->GetType());
  BrowserDistribution* chrome =
      installer_state.products()[0]->distribution();
  EXPECT_EQ(BrowserDistribution::CHROME_BROWSER, chrome->GetType());

  // By default, eulaconsent ends up on the package.
  EXPECT_TRUE(GoogleUpdateSettings::SetEULAConsent(installer_state, true));
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_LOCAL_MACHINE, binaries->GetStateMediumKey().c_str(),
               KEY_QUERY_VALUE | KEY_SET_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
      key.ReadValueDW(google_update::kRegEULAAceptedField, &value));
  EXPECT_EQ(1U, value);
  EXPECT_EQ(ERROR_SUCCESS,
      key.DeleteValue(google_update::kRegEULAAceptedField));

  // But it will end up on the product if needed
  EXPECT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_LOCAL_MACHINE, chrome->GetStateKey().c_str(),
                 KEY_SET_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
      key.WriteValue(google_update::kRegEULAAceptedField,
                     static_cast<DWORD>(0)));
  EXPECT_TRUE(GoogleUpdateSettings::SetEULAConsent(installer_state, true));
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_LOCAL_MACHINE, chrome->GetStateMediumKey().c_str(),
               KEY_QUERY_VALUE | KEY_SET_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
      key.ReadValueDW(google_update::kRegEULAAceptedField, &value));
  EXPECT_EQ(1U, value);
}

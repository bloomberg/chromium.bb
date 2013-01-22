// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>  // For SHDeleteKey.

#include "base/memory/scoped_ptr.h"
#include "base/test/test_reg_util_win.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/fake_installation_state.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using installer::ChannelInfo;

namespace {

const wchar_t kGoogleUpdatePoliciesKey[] =
    L"SOFTWARE\\Policies\\Google\\Update";
const wchar_t kGoogleUpdateUpdateDefault[] = L"UpdateDefault";
const wchar_t kGoogleUpdateUpdatePrefix[] = L"Update";
const GoogleUpdateSettings::UpdatePolicy kDefaultUpdatePolicy =
#if defined(GOOGLE_CHROME_BUILD)
    GoogleUpdateSettings::AUTOMATIC_UPDATES;
#else
    GoogleUpdateSettings::UPDATES_DISABLED;
#endif

const wchar_t kTestProductGuid[] = L"{89F1B351-B15D-48D4-8F10-1298721CF13D}";
const wchar_t kTestExperimentLabel[] = L"test_label_value";

// This test fixture redirects the HKLM and HKCU registry hives for
// the duration of the test to make it independent of the machine
// and user settings.
class GoogleUpdateSettingsTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    registry_overrides_.OverrideRegistry(HKEY_LOCAL_MACHINE, L"HKLM_pit");
    registry_overrides_.OverrideRegistry(HKEY_CURRENT_USER, L"HKCU_pit");
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
      { L"dev", installer::kChromeChannelDev },
      { L"-dev", installer::kChromeChannelDev },
      { L"-developer", installer::kChromeChannelDev },
      { L"beta", installer::kChromeChannelBeta },
      { L"-beta", installer::kChromeChannelBeta },
      { L"-betamax", installer::kChromeChannelBeta },
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
          string16 ret_channel;

          EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(
              is_system, &ret_channel));
          EXPECT_STREQ(channel, ret_channel.c_str())
              << "Expecting channel \"" << channel
              << "\" for ap=\"" << ap << "\"";
        }
      }
    }
  }

  // Test the writing and deleting functionality of the experiments label
  // helper.
  void TestExperimentsLabelHelper(SystemUserInstall install) {
    BrowserDistribution* chrome =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    std::wstring value;
#if defined(GOOGLE_CHROME_BUILD)
    EXPECT_TRUE(chrome->ShouldSetExperimentLabels());

    // Before anything is set, ReadExperimentLabels should succeed but return
    // an empty string.
    EXPECT_TRUE(GoogleUpdateSettings::ReadExperimentLabels(
        install == SYSTEM_INSTALL, &value));
    EXPECT_EQ(string16(), value);

    EXPECT_TRUE(GoogleUpdateSettings::SetExperimentLabels(
        install == SYSTEM_INSTALL, kTestExperimentLabel));

    // Validate that something is written. Only worry about the label itself.
    RegKey key;
    HKEY root = install == SYSTEM_INSTALL ?
        HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    string16 state_key = install == SYSTEM_INSTALL ?
        chrome->GetStateMediumKey() : chrome->GetStateKey();

    EXPECT_EQ(ERROR_SUCCESS,
              key.Open(root, state_key.c_str(), KEY_QUERY_VALUE));
    EXPECT_EQ(ERROR_SUCCESS,
        key.ReadValue(google_update::kExperimentLabels, &value));
    EXPECT_EQ(kTestExperimentLabel, value);
    EXPECT_TRUE(GoogleUpdateSettings::ReadExperimentLabels(
        install == SYSTEM_INSTALL, &value));
    EXPECT_EQ(kTestExperimentLabel, value);
    key.Close();

    // Now that the label is set, test the delete functionality. An empty label
    // should result in deleting the value.
    EXPECT_TRUE(GoogleUpdateSettings::SetExperimentLabels(
        install == SYSTEM_INSTALL, string16()));
    EXPECT_EQ(ERROR_SUCCESS,
              key.Open(root, state_key.c_str(), KEY_QUERY_VALUE));
    EXPECT_EQ(ERROR_FILE_NOT_FOUND,
        key.ReadValue(google_update::kExperimentLabels, &value));
    EXPECT_TRUE(GoogleUpdateSettings::ReadExperimentLabels(
        install == SYSTEM_INSTALL, &value));
    EXPECT_EQ(string16(), value);
    key.Close();
#else
    EXPECT_FALSE(chrome->ShouldSetExperimentLabels());
    EXPECT_FALSE(GoogleUpdateSettings::ReadExperimentLabels(
        install == SYSTEM_INSTALL, &value));
#endif  // GOOGLE_CHROME_BUILD
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

  registry_util::RegistryOverrideManager registry_overrides_;
};

}  // namespace

// Verify that we return success on no registration (which means stable),
// whether per-system or per-user install.
TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelAbsent) {
  // Per-system first.
  string16 channel;
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(true,
                                                                 &channel));
  EXPECT_STREQ(L"", channel.c_str());

  // Then per-user.
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(false,
                                                                 &channel));
  EXPECT_STREQ(L"", channel.c_str());
}

// Test an empty Ap key for system and user.
TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelEmptySystem) {
  SetApField(SYSTEM_INSTALL, L"");
  string16 channel;
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(true,
                                                                 &channel));
  EXPECT_STREQ(L"", channel.c_str());

  // Per-user lookups still succeed and return empty string.
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(false,
                                                                 &channel));
  EXPECT_STREQ(L"", channel.c_str());
}

TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelEmptyUser) {
  SetApField(USER_INSTALL, L"");
  // Per-system lookups still succeed and return empty string.
  string16 channel;
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(true,
                                                                 &channel));
  EXPECT_STREQ(L"", channel.c_str());

  // Per-user lookup should succeed.
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(false,
                                                                 &channel));
  EXPECT_STREQ(L"", channel.c_str());
}

TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelVariousApValuesSystem) {
  TestCurrentChromeChannelWithVariousApValues(SYSTEM_INSTALL);
}

TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelVariousApValuesUser) {
  TestCurrentChromeChannelWithVariousApValues(USER_INSTALL);
}

// Run through all combinations of diff vs. full install, single vs. multi
// install, success and failure results, and a fistful of initial "ap" values
// checking that the expected final "ap" value is generated by
// GoogleUpdateSettings::UpdateGoogleUpdateApKey.
TEST_F(GoogleUpdateSettingsTest, UpdateGoogleUpdateApKey) {
  const installer::ArchiveType archive_types[] = {
    installer::UNKNOWN_ARCHIVE_TYPE,
    installer::FULL_ARCHIVE_TYPE,
    installer::INCREMENTAL_ARCHIVE_TYPE
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
  for (int type_idx = 0; type_idx < arraysize(archive_types); ++type_idx) {
    const installer::ArchiveType archive_type = archive_types[type_idx];
    for (int result_idx = 0; result_idx < arraysize(results); ++result_idx) {
      const int result = results[result_idx];
      // The archive type will/must always be known on install success.
      if (archive_type == installer::UNKNOWN_ARCHIVE_TYPE &&
          result == installer::FIRST_INSTALL_SUCCESS) {
        continue;
      }
      const wchar_t* const* outputs = NULL;
      if (result == installer::FIRST_INSTALL_SUCCESS ||
          archive_type == installer::FULL_ARCHIVE_TYPE) {
        outputs = plain;
      } else if (archive_type == installer::INCREMENTAL_ARCHIVE_TYPE) {
        outputs = full;
      }  // else if (archive_type == UNKNOWN) see below

      for (int inputs_idx = 0; inputs_idx < arraysize(input_arrays);
           ++inputs_idx) {
        const wchar_t* const* inputs = input_arrays[inputs_idx];
        if (archive_type == installer::UNKNOWN_ARCHIVE_TYPE) {
          // "-full" is untouched if the archive type is unknown.
          // "-multifail" is unconditionally removed.
          if (inputs == full || inputs == multifail_full)
            outputs = full;
          else
            outputs = plain;
        }
        for (int input_idx = 0; input_idx < arraysize(plain); ++input_idx) {
          const wchar_t* input = inputs[input_idx];
          const wchar_t* output = outputs[input_idx];

          v.set_value(input);
          if (output == v.value()) {
            EXPECT_FALSE(GoogleUpdateSettings::UpdateGoogleUpdateApKey(
                archive_type, result, &v))
                << "archive_type: " << archive_type
                << ", result: " << result
                << ", input ap value: " << input;
          } else {
            EXPECT_TRUE(GoogleUpdateSettings::UpdateGoogleUpdateApKey(
                archive_type, result, &v))
                << "archive_type: " << archive_type
                << ", result: " << result
                << ", input ap value: " << input;
          }
          EXPECT_EQ(output, v.value())
              << "archive_type: " << archive_type
              << ", result: " << result
              << ", input ap value: " << input;
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
  GoogleUpdateSettings::UpdateInstallStatus(false,
                                            installer::INCREMENTAL_ARCHIVE_TYPE,
                                            installer::INSTALL_FAILED,
                                            kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test incremental install success
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L""))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false,
                                            installer::INCREMENTAL_ARCHIVE_TYPE,
                                            installer::FIRST_INSTALL_SUCCESS,
                                            kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install failure
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L"-full"))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false, installer::FULL_ARCHIVE_TYPE,
                                            installer::INSTALL_FAILED,
                                            kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install success
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L"-full"))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false, installer::FULL_ARCHIVE_TYPE,
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
  GoogleUpdateSettings::UpdateInstallStatus(false,
                                            installer::INCREMENTAL_ARCHIVE_TYPE,
                                            installer::INSTALL_FAILED,
                                            kTestProductGuid);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  // try full installer now
  GoogleUpdateSettings::UpdateInstallStatus(false, installer::FULL_ARCHIVE_TYPE,
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
  using installer::FakeInstallationState;

  const bool multi_install = true;
  const bool system_level = true;
  FakeInstallationState machine_state;

  // Chrome is installed.
  machine_state.AddChrome(system_level, multi_install,
      new Version(chrome::kChromeVersion));

  RegKey key;
  DWORD value;
  BrowserDistribution* binaries =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BINARIES);
  BrowserDistribution* chrome =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER);

  // eulaconsent is set on both the product and the binaries.
  EXPECT_TRUE(GoogleUpdateSettings::SetEULAConsent(machine_state, chrome,
                                                   true));
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_LOCAL_MACHINE, binaries->GetStateMediumKey().c_str(),
               KEY_QUERY_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
      key.ReadValueDW(google_update::kRegEULAAceptedField, &value));
  EXPECT_EQ(1U, value);
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_LOCAL_MACHINE, chrome->GetStateMediumKey().c_str(),
               KEY_QUERY_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
      key.ReadValueDW(google_update::kRegEULAAceptedField, &value));
  EXPECT_EQ(1U, value);
}

// Test that the appropriate default is returned if no update override is
// present.
TEST_F(GoogleUpdateSettingsTest, GetAppUpdatePolicyNoOverride) {
  // There are no policies at all.
  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            RegKey().Open(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                          KEY_QUERY_VALUE));
  bool is_overridden = true;
  EXPECT_EQ(kDefaultUpdatePolicy,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  // The policy key exists, but there are no values of interest present.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey().Create(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                            KEY_SET_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey().Open(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                          KEY_QUERY_VALUE));
  is_overridden = true;
  EXPECT_EQ(kDefaultUpdatePolicy,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);
}

#if defined(GOOGLE_CHROME_BUILD)

// Test that the default override is returned if no app-specific override is
// present.
TEST_F(GoogleUpdateSettingsTest, GetAppUpdatePolicyDefaultOverride) {
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(kGoogleUpdateUpdateDefault,
                                             static_cast<DWORD>(0)));
  bool is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(kGoogleUpdateUpdateDefault,
                                             static_cast<DWORD>(1)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(kGoogleUpdateUpdateDefault,
                                             static_cast<DWORD>(2)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::MANUAL_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  // The default policy should be in force for bogus values.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(kGoogleUpdateUpdateDefault,
                                             static_cast<DWORD>(3)));
  is_overridden = true;
  EXPECT_EQ(kDefaultUpdatePolicy,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);
}

// Test that an app-specific override is used if present.
TEST_F(GoogleUpdateSettingsTest, GetAppUpdatePolicyAppOverride) {
  std::wstring app_policy_value(kGoogleUpdateUpdatePrefix);
  app_policy_value.append(kTestProductGuid);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(kGoogleUpdateUpdateDefault,
                                             static_cast<DWORD>(1)));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(app_policy_value.c_str(),
                                             static_cast<DWORD>(0)));
  bool is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(kGoogleUpdateUpdateDefault,
                                             static_cast<DWORD>(0)));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(app_policy_value.c_str(),
                                             static_cast<DWORD>(1)));
  is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(app_policy_value.c_str(),
                                             static_cast<DWORD>(2)));
  is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::MANUAL_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  // The default policy should be in force for bogus values.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                   KEY_SET_VALUE).WriteValue(app_policy_value.c_str(),
                                             static_cast<DWORD>(3)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);
}

TEST_F(GoogleUpdateSettingsTest, ExperimentsLabelHelperSystem) {
  TestExperimentsLabelHelper(SYSTEM_INSTALL);
}

TEST_F(GoogleUpdateSettingsTest, ExperimentsLabelHelperUser) {
  TestExperimentsLabelHelper(USER_INSTALL);
}

#endif  // defined(GOOGLE_CHROME_BUILD)

// Test GoogleUpdateSettings::GetUninstallCommandLine at system- or user-level,
// according to the param.
class GetUninstallCommandLine : public GoogleUpdateSettingsTest,
                                public testing::WithParamInterface<bool> {
 protected:
  static const wchar_t kDummyCommand[];

  virtual void SetUp() OVERRIDE {
    GoogleUpdateSettingsTest::SetUp();
    system_install_ = GetParam();
    root_key_ = system_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  }

  HKEY root_key_;
  bool system_install_;
};

const wchar_t GetUninstallCommandLine::kDummyCommand[] =
    L"\"goopdate.exe\" /spam";

// Tests that GetUninstallCommandLine returns an empty string if there's no
// Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestNoKey) {
  EXPECT_EQ(string16(),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
}

// Tests that GetUninstallCommandLine returns an empty string if there's no
// UninstallCmdLine value in the Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestNoValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE);
  EXPECT_EQ(string16(),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
}

// Tests that GetUninstallCommandLine returns an empty string if there's an
// empty UninstallCmdLine value in the Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestEmptyValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
    .WriteValue(google_update::kRegUninstallCmdLine, L"");
  EXPECT_EQ(string16(),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
}

// Tests that GetUninstallCommandLine returns the correct string if there's an
// UninstallCmdLine value in the Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestRealValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
      .WriteValue(google_update::kRegUninstallCmdLine, kDummyCommand);
  EXPECT_EQ(string16(kDummyCommand),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
  // Make sure that there's no value in the other level (user or system).
  EXPECT_EQ(string16(),
            GoogleUpdateSettings::GetUninstallCommandLine(!system_install_));
}

INSTANTIATE_TEST_CASE_P(GetUninstallCommandLineAtLevel, GetUninstallCommandLine,
                        testing::Bool());

// Test GoogleUpdateSettings::GetGoogleUpdateVersion at system- or user-level,
// according to the param.
class GetGoogleUpdateVersion : public GoogleUpdateSettingsTest,
                               public testing::WithParamInterface<bool> {
 protected:
  static const wchar_t kDummyVersion[];

  virtual void SetUp() OVERRIDE {
    GoogleUpdateSettingsTest::SetUp();
    system_install_ = GetParam();
    root_key_ = system_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  }

  HKEY root_key_;
  bool system_install_;
};

const wchar_t GetGoogleUpdateVersion::kDummyVersion[] = L"1.2.3.4";

// Tests that GetGoogleUpdateVersion returns an empty string if there's no
// Software\Google\Update key.
TEST_P(GetGoogleUpdateVersion, TestNoKey) {
  EXPECT_FALSE(
      GoogleUpdateSettings::GetGoogleUpdateVersion(system_install_).IsValid());
}

// Tests that GetGoogleUpdateVersion returns an empty string if there's no
// version value in the Software\Google\Update key.
TEST_P(GetGoogleUpdateVersion, TestNoValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE);
  EXPECT_FALSE(
      GoogleUpdateSettings::GetGoogleUpdateVersion(system_install_).IsValid());
}

// Tests that GetGoogleUpdateVersion returns an empty string if there's an
// empty version value in the Software\Google\Update key.
TEST_P(GetGoogleUpdateVersion, TestEmptyValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
      .WriteValue(google_update::kRegGoogleUpdateVersion, L"");
  EXPECT_FALSE(
      GoogleUpdateSettings::GetGoogleUpdateVersion(system_install_).IsValid());
}

// Tests that GetGoogleUpdateVersion returns the correct string if there's a
// version value in the Software\Google\Update key.
TEST_P(GetGoogleUpdateVersion, TestRealValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
      .WriteValue(google_update::kRegGoogleUpdateVersion, kDummyVersion);
  Version expected(UTF16ToUTF8(kDummyVersion));
  EXPECT_TRUE(expected.Equals(
      GoogleUpdateSettings::GetGoogleUpdateVersion(system_install_)));
  // Make sure that there's no value in the other level (user or system).
  EXPECT_FALSE(
      GoogleUpdateSettings::GetGoogleUpdateVersion(!system_install_)
          .IsValid());
}

INSTANTIATE_TEST_CASE_P(GetGoogleUpdateVersionAtLevel, GetGoogleUpdateVersion,
                        testing::Bool());

// Test values for use by the CollectStatsConsent test fixture.
class StatsState {
 public:
  enum InstallType {
    SINGLE_INSTALL,
    MULTI_INSTALL,
  };
  enum StateSetting {
    NO_SETTING,
    FALSE_SETTING,
    TRUE_SETTING,
  };
  struct UserLevelState {};
  struct SystemLevelState {};
  static const UserLevelState kUserLevel;
  static const SystemLevelState kSystemLevel;

  StatsState(const UserLevelState&,
             InstallType install_type,
             StateSetting state_value)
      : system_level_(false),
        multi_install_(install_type == MULTI_INSTALL),
        state_value_(state_value),
        state_medium_value_(NO_SETTING) {
  }
  StatsState(const SystemLevelState&,
             InstallType install_type,
             StateSetting state_value,
             StateSetting state_medium_value)
      : system_level_(true),
        multi_install_(install_type == MULTI_INSTALL),
        state_value_(state_value),
        state_medium_value_(state_medium_value) {
  }
  bool system_level() const { return system_level_; }
  bool multi_install() const { return multi_install_; }
  HKEY root_key() const {
    return system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  }
  StateSetting state_value() const { return state_value_; }
  StateSetting state_medium_value() const {
    return state_medium_value_;
  }
  bool is_consent_granted() const {
    return (system_level_ && state_medium_value_ != NO_SETTING) ?
        (state_medium_value_ == TRUE_SETTING) :
        (state_value_ == TRUE_SETTING);
  }
 private:
  bool system_level_;
  bool multi_install_;
  StateSetting state_value_;
  StateSetting state_medium_value_;
};

const StatsState::UserLevelState StatsState::kUserLevel;
const StatsState::SystemLevelState StatsState::kSystemLevel;

// A value parameterized test for testing the stats collection consent setting.
class CollectStatsConsent : public ::testing::TestWithParam<StatsState> {
 public:
  static void SetUpTestCase();
  static void TearDownTestCase();
 protected:
  virtual void SetUp() OVERRIDE;
  static void MakeChromeMultiInstall(HKEY root_key);
  static void ApplySetting(StatsState::StateSetting setting,
                           HKEY root_key,
                           const std::wstring& reg_key);

  static std::wstring* chrome_version_key_;
  static std::wstring* chrome_state_key_;
  static std::wstring* chrome_state_medium_key_;
  static std::wstring* binaries_state_key_;
  static std::wstring* binaries_state_medium_key_;
  registry_util::RegistryOverrideManager override_manager_;
};

std::wstring* CollectStatsConsent::chrome_version_key_;
std::wstring* CollectStatsConsent::chrome_state_key_;
std::wstring* CollectStatsConsent::chrome_state_medium_key_;
std::wstring* CollectStatsConsent::binaries_state_key_;
std::wstring* CollectStatsConsent::binaries_state_medium_key_;

void CollectStatsConsent::SetUpTestCase() {
  BrowserDistribution* dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER);
  chrome_version_key_ = new std::wstring(dist->GetVersionKey());
  chrome_state_key_ = new std::wstring(dist->GetStateKey());
  chrome_state_medium_key_ = new std::wstring(dist->GetStateMediumKey());

  dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES);
  binaries_state_key_ = new std::wstring(dist->GetStateKey());
  binaries_state_medium_key_ = new std::wstring(dist->GetStateMediumKey());
}

void CollectStatsConsent::TearDownTestCase() {
  delete chrome_version_key_;
  delete chrome_state_key_;
  delete chrome_state_medium_key_;
  delete binaries_state_key_;
  delete binaries_state_medium_key_;
}

// Install the registry override and apply the settings to the registry.
void CollectStatsConsent::SetUp() {
  const StatsState& stats_state = GetParam();
  const HKEY root_key = stats_state.root_key();
  std::wstring reg_temp_name(stats_state.system_level() ? L"HKLM_" : L"HKCU_");
  reg_temp_name += L"CollectStatsConsent";
  override_manager_.OverrideRegistry(root_key, reg_temp_name);

  if (stats_state.multi_install()) {
    MakeChromeMultiInstall(root_key);
    ApplySetting(stats_state.state_value(), root_key, *binaries_state_key_);
    ApplySetting(stats_state.state_medium_value(), root_key,
                 *binaries_state_medium_key_);
  } else {
    ApplySetting(stats_state.state_value(), root_key, *chrome_state_key_);
    ApplySetting(stats_state.state_medium_value(), root_key,
                 *chrome_state_medium_key_);
  }
}

// Write values into the registry so that Chrome is considered to be installed
// as multi-install.
void CollectStatsConsent::MakeChromeMultiInstall(HKEY root_key) {
  ASSERT_EQ(
      ERROR_SUCCESS,
      RegKey(root_key, chrome_version_key_->c_str(),
             KEY_SET_VALUE).WriteValue(google_update::kRegVersionField,
                                       L"1.2.3.4"));
  ASSERT_EQ(
      ERROR_SUCCESS,
      RegKey(root_key, chrome_state_key_->c_str(),
             KEY_SET_VALUE).WriteValue(installer::kUninstallArgumentsField,
                                       L"--multi-install"));
}

// Write the correct value to represent |setting| in the registry.
void CollectStatsConsent::ApplySetting(StatsState::StateSetting setting,
                                       HKEY root_key,
                                       const std::wstring& reg_key) {
  if (setting != StatsState::NO_SETTING) {
    DWORD value = setting != StatsState::FALSE_SETTING ? 1 : 0;
    ASSERT_EQ(
        ERROR_SUCCESS,
        RegKey(root_key, reg_key.c_str(),
               KEY_SET_VALUE).WriteValue(google_update::kRegUsageStatsField,
                                         value));
  }
}

// Test that stats consent can be read.
TEST_P(CollectStatsConsent, GetCollectStatsConsentAtLevel) {
  if (GetParam().is_consent_granted()) {
    EXPECT_TRUE(GoogleUpdateSettings::GetCollectStatsConsentAtLevel(
                    GetParam().system_level()));
  } else {
    EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsentAtLevel(
                     GetParam().system_level()));
  }
}

// Test that stats consent can be flipped to the opposite setting, that the new
// setting takes affect, and that the correct registry location is modified.
TEST_P(CollectStatsConsent, SetCollectStatsConsentAtLevel) {
  EXPECT_TRUE(GoogleUpdateSettings::SetCollectStatsConsentAtLevel(
                  GetParam().system_level(),
                  !GetParam().is_consent_granted()));
  const std::wstring* const reg_keys[] = {
    chrome_state_key_,
    chrome_state_medium_key_,
    binaries_state_key_,
    binaries_state_medium_key_,
  };
  int key_index = ((GetParam().system_level() ? 1 : 0) +
                   (GetParam().multi_install() ? 2 : 0));
  const std::wstring& reg_key = *reg_keys[key_index];
  DWORD value = 0;
  EXPECT_EQ(
      ERROR_SUCCESS,
      RegKey(GetParam().root_key(), reg_key.c_str(),
             KEY_QUERY_VALUE).ReadValueDW(google_update::kRegUsageStatsField,
                                          &value));
  if (GetParam().is_consent_granted()) {
    EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsentAtLevel(
                     GetParam().system_level()));
    EXPECT_EQ(0UL, value);
  } else {
    EXPECT_TRUE(GoogleUpdateSettings::GetCollectStatsConsentAtLevel(
                    GetParam().system_level()));
    EXPECT_EQ(1UL, value);
  }
}

INSTANTIATE_TEST_CASE_P(
    UserLevelSingleInstall,
    CollectStatsConsent,
    ::testing::Values(
        StatsState(StatsState::kUserLevel, StatsState::SINGLE_INSTALL,
                   StatsState::NO_SETTING),
        StatsState(StatsState::kUserLevel, StatsState::SINGLE_INSTALL,
                   StatsState::FALSE_SETTING),
        StatsState(StatsState::kUserLevel, StatsState::SINGLE_INSTALL,
                   StatsState::TRUE_SETTING)));
INSTANTIATE_TEST_CASE_P(
    UserLevelMultiInstall,
    CollectStatsConsent,
    ::testing::Values(
        StatsState(StatsState::kUserLevel, StatsState::MULTI_INSTALL,
                   StatsState::NO_SETTING),
        StatsState(StatsState::kUserLevel, StatsState::MULTI_INSTALL,
                   StatsState::FALSE_SETTING),
        StatsState(StatsState::kUserLevel, StatsState::MULTI_INSTALL,
                   StatsState::TRUE_SETTING)));
INSTANTIATE_TEST_CASE_P(
    SystemLevelSingleInstall,
    CollectStatsConsent,
    ::testing::Values(
        StatsState(StatsState::kSystemLevel, StatsState::SINGLE_INSTALL,
                   StatsState::NO_SETTING, StatsState::NO_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::SINGLE_INSTALL,
                   StatsState::NO_SETTING, StatsState::FALSE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::SINGLE_INSTALL,
                   StatsState::NO_SETTING, StatsState::TRUE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::SINGLE_INSTALL,
                   StatsState::FALSE_SETTING, StatsState::NO_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::SINGLE_INSTALL,
                   StatsState::FALSE_SETTING, StatsState::FALSE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::SINGLE_INSTALL,
                   StatsState::FALSE_SETTING, StatsState::TRUE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::SINGLE_INSTALL,
                   StatsState::TRUE_SETTING, StatsState::NO_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::SINGLE_INSTALL,
                   StatsState::TRUE_SETTING, StatsState::FALSE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::SINGLE_INSTALL,
                   StatsState::TRUE_SETTING, StatsState::TRUE_SETTING)));
INSTANTIATE_TEST_CASE_P(
    SystemLevelMultiInstall,
    CollectStatsConsent,
    ::testing::Values(
        StatsState(StatsState::kSystemLevel, StatsState::MULTI_INSTALL,
                   StatsState::NO_SETTING, StatsState::NO_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::MULTI_INSTALL,
                   StatsState::NO_SETTING, StatsState::FALSE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::MULTI_INSTALL,
                   StatsState::NO_SETTING, StatsState::TRUE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::MULTI_INSTALL,
                   StatsState::FALSE_SETTING, StatsState::NO_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::MULTI_INSTALL,
                   StatsState::FALSE_SETTING, StatsState::FALSE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::MULTI_INSTALL,
                   StatsState::FALSE_SETTING, StatsState::TRUE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::MULTI_INSTALL,
                   StatsState::TRUE_SETTING, StatsState::NO_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::MULTI_INSTALL,
                   StatsState::TRUE_SETTING, StatsState::FALSE_SETTING),
        StatsState(StatsState::kSystemLevel, StatsState::MULTI_INSTALL,
                   StatsState::TRUE_SETTING, StatsState::TRUE_SETTING)));

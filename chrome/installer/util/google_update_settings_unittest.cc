// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include <windows.h>
#include <shlwapi.h>  // For SHDeleteKey.
#include <stddef.h>

#include "base/base_paths.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/fake_installation_state.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using installer::ChannelInfo;

namespace {

const wchar_t kTestProductGuid[] = L"{89F1B351-B15D-48D4-8F10-1298721CF13D}";

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kTestExperimentLabel[] = L"test_label_value";
#endif

// This test fixture redirects the HKLM and HKCU registry hives for
// the duration of the test to make it independent of the machine
// and user settings.
class GoogleUpdateSettingsTest : public testing::Test {
 protected:
  enum SystemUserInstall {
    SYSTEM_INSTALL,
    USER_INSTALL,
  };

  GoogleUpdateSettingsTest()
      : program_files_override_(base::DIR_PROGRAM_FILES),
        program_files_x86_override_(base::DIR_PROGRAM_FILESX86) {
    registry_overrides_.OverrideRegistry(HKEY_LOCAL_MACHINE);
    registry_overrides_.OverrideRegistry(HKEY_CURRENT_USER);
  }

  void SetApField(SystemUserInstall is_system, const wchar_t* value) {
    HKEY root = is_system == SYSTEM_INSTALL ?
        HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    RegKey update_key;
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    base::string16 path = dist->GetStateKey();
    ASSERT_EQ(ERROR_SUCCESS, update_key.Create(root, path.c_str(), KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS, update_key.WriteValue(L"ap", value));
  }

  // Sets the "ap" field for a multi-install product (both the product and
  // the binaries).
  void SetMultiApField(SystemUserInstall is_system, const wchar_t* value) {
    // Caller must specify a multi-install ap value.
    ASSERT_NE(base::string16::npos, base::string16(value).find(L"-multi"));
    HKEY root = is_system == SYSTEM_INSTALL ?
        HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    RegKey update_key;

    // Write the ap value for both the product and the binaries.
    BrowserDistribution* const kDists[] = {
      BrowserDistribution::GetDistribution(),
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BINARIES)
    };
    for (size_t i = 0; i < arraysize(kDists); ++i) {
      base::string16 path = kDists[i]->GetStateKey();
      ASSERT_EQ(ERROR_SUCCESS, update_key.Create(root, path.c_str(),
                                                 KEY_WRITE));
      ASSERT_EQ(ERROR_SUCCESS, update_key.WriteValue(L"ap", value));
    }

    // Make the product technically multi-install.
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    ASSERT_EQ(ERROR_SUCCESS,
              update_key.Create(root, dist->GetStateKey().c_str(), KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS,
              update_key.WriteValue(installer::kUninstallArgumentsField,
                                    L"--multi-install"));
  }

  // Tests setting the ap= value to various combinations of values with
  // suffixes, while asserting on the correct channel value.
  // Note that ap= value has to match "^2.0-d.*" or ".*x64-dev.*" and "^1.1-.*"
  // or ".*x64-beta.*" for dev and beta channels respectively.
  void TestCurrentChromeChannelWithVariousApValues(SystemUserInstall install) {
    static struct Expectation {
      const wchar_t* ap_value;
      const wchar_t* channel;
      bool supports_prefixes;
    } expectations[] = {
      { L"2.0-dev", installer::kChromeChannelDev, false},
      { L"1.1-beta", installer::kChromeChannelBeta, false},
      { L"x64-dev", installer::kChromeChannelDev, true},
      { L"x64-beta", installer::kChromeChannelBeta, true},
      { L"x64-stable", installer::kChromeChannelStable, true},
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

    for (const wchar_t* prefix : prefixes) {
      for (const Expectation& expectation : expectations) {
        for (const wchar_t* suffix : suffixes) {
          base::string16 ap = prefix;
          ap += expectation.ap_value;
          ap += suffix;
          const wchar_t* channel = expectation.channel;

          SetApField(install, ap.c_str());
          base::string16 ret_channel;

          EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(
            is_system, &ret_channel));

          // If prefixes are not supported for a channel, we expect the channel
          // to be "unknown" if a non-empty prefix is present in ap_value.
          if (!expectation.supports_prefixes && wcslen(prefix) > 0) {
            EXPECT_STREQ(installer::kChromeChannelUnknown, ret_channel.c_str())
                << "Expecting channel \"" << installer::kChromeChannelUnknown
                << "\" for ap=\"" << ap << "\"";
          } else {
            EXPECT_STREQ(channel, ret_channel.c_str())
                << "Expecting channel \"" << channel
                << "\" for ap=\"" << ap << "\"";
          }
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
    base::string16 value;
#if defined(GOOGLE_CHROME_BUILD)
    EXPECT_TRUE(chrome->ShouldSetExperimentLabels());

    // Before anything is set, ReadExperimentLabels should succeed but return
    // an empty string.
    EXPECT_TRUE(GoogleUpdateSettings::ReadExperimentLabels(
        install == SYSTEM_INSTALL, &value));
    EXPECT_EQ(base::string16(), value);

    EXPECT_TRUE(GoogleUpdateSettings::SetExperimentLabels(
        install == SYSTEM_INSTALL, kTestExperimentLabel));

    // Validate that something is written. Only worry about the label itself.
    RegKey key;
    HKEY root = install == SYSTEM_INSTALL ?
        HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    base::string16 state_key = install == SYSTEM_INSTALL ?
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
        install == SYSTEM_INSTALL, base::string16()));
    EXPECT_EQ(ERROR_SUCCESS,
              key.Open(root, state_key.c_str(), KEY_QUERY_VALUE));
    EXPECT_EQ(ERROR_FILE_NOT_FOUND,
        key.ReadValue(google_update::kExperimentLabels, &value));
    EXPECT_TRUE(GoogleUpdateSettings::ReadExperimentLabels(
        install == SYSTEM_INSTALL, &value));
    EXPECT_EQ(base::string16(), value);
    key.Close();
#else
    EXPECT_FALSE(chrome->ShouldSetExperimentLabels());
    EXPECT_FALSE(GoogleUpdateSettings::ReadExperimentLabels(
        install == SYSTEM_INSTALL, &value));
#endif  // GOOGLE_CHROME_BUILD
  }

  // Creates "ap" key with the value given as parameter. Also adds work
  // items to work_item_list given so that they can be rolled back later.
  bool CreateApKey(WorkItemList* work_item_list, const base::string16& value) {
    HKEY reg_root = HKEY_CURRENT_USER;
    base::string16 reg_key = GetApKeyPath();
    work_item_list->AddCreateRegKeyWorkItem(
        reg_root, reg_key, WorkItem::kWow64Default);
    work_item_list->AddSetRegValueWorkItem(reg_root,
                                           reg_key,
                                           WorkItem::kWow64Default,
                                           google_update::kRegApField,
                                           value.c_str(),
                                           true);
    if (!work_item_list->Do()) {
      work_item_list->Rollback();
      return false;
    }
    return true;
  }

  // Returns the key path of "ap" key, e.g.:
  // Google\Update\ClientState\<kTestProductGuid>
  base::string16 GetApKeyPath() {
    base::string16 reg_key(google_update::kRegPathClientState);
    reg_key.append(L"\\");
    reg_key.append(kTestProductGuid);
    return reg_key;
  }

  // Utility method to read "ap" key value
  base::string16 ReadApKeyValue() {
    RegKey key;
    base::string16 ap_key_value;
    base::string16 reg_key = GetApKeyPath();
    if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS) ==
        ERROR_SUCCESS) {
      key.ReadValue(google_update::kRegApField, &ap_key_value);
    }

    return ap_key_value;
  }

  bool SetUpdatePolicyForAppGuid(const base::string16& app_guid,
                                 GoogleUpdateSettings::UpdatePolicy policy) {
    RegKey policy_key;
    if (policy_key.Create(HKEY_LOCAL_MACHINE,
                          GoogleUpdateSettings::kPoliciesKey,
                          KEY_SET_VALUE) == ERROR_SUCCESS) {
      base::string16 app_update_override(
          GoogleUpdateSettings::kUpdateOverrideValuePrefix);
      app_update_override.append(app_guid);
      return policy_key.WriteValue(app_update_override.c_str(),
                                   static_cast<DWORD>(policy)) == ERROR_SUCCESS;
    }
    return false;
  }

  GoogleUpdateSettings::UpdatePolicy GetUpdatePolicyForAppGuid(
      const base::string16& app_guid) {
    RegKey policy_key;
    if (policy_key.Create(HKEY_LOCAL_MACHINE,
                          GoogleUpdateSettings::kPoliciesKey,
                          KEY_QUERY_VALUE) == ERROR_SUCCESS) {
      base::string16 app_update_override(
          GoogleUpdateSettings::kUpdateOverrideValuePrefix);
      app_update_override.append(app_guid);

      DWORD value;
      if (policy_key.ReadValueDW(app_update_override.c_str(),
                                 &value) == ERROR_SUCCESS) {
        return static_cast<GoogleUpdateSettings::UpdatePolicy>(value);
      }
    }
    return GoogleUpdateSettings::UPDATE_POLICIES_COUNT;
  }

  bool SetGlobalUpdatePolicy(GoogleUpdateSettings::UpdatePolicy policy) {
    RegKey policy_key;
    return policy_key.Create(HKEY_LOCAL_MACHINE,
                             GoogleUpdateSettings::kPoliciesKey,
                             KEY_SET_VALUE) == ERROR_SUCCESS &&
           policy_key.WriteValue(GoogleUpdateSettings::kUpdatePolicyValue,
                                 static_cast<DWORD>(policy)) == ERROR_SUCCESS;
  }

  GoogleUpdateSettings::UpdatePolicy GetGlobalUpdatePolicy() {
    RegKey policy_key;
    DWORD value;
    return (policy_key.Create(HKEY_LOCAL_MACHINE,
                              GoogleUpdateSettings::kPoliciesKey,
                              KEY_QUERY_VALUE) == ERROR_SUCCESS &&
            policy_key.ReadValueDW(GoogleUpdateSettings::kUpdatePolicyValue,
                                   &value) == ERROR_SUCCESS) ?
        static_cast<GoogleUpdateSettings::UpdatePolicy>(value) :
        GoogleUpdateSettings::UPDATE_POLICIES_COUNT;
  }

  bool SetUpdateTimeoutOverride(DWORD time_in_minutes) {
    RegKey policy_key;
    return policy_key.Create(HKEY_LOCAL_MACHINE,
                             GoogleUpdateSettings::kPoliciesKey,
                             KEY_SET_VALUE) == ERROR_SUCCESS &&
           policy_key.WriteValue(
               GoogleUpdateSettings::kCheckPeriodOverrideMinutes,
               time_in_minutes) == ERROR_SUCCESS;
  }

  // Path overrides so that SHGetFolderPath isn't needed after the registry
  // is overridden.
  base::ScopedPathOverride program_files_override_;
  base::ScopedPathOverride program_files_x86_override_;
  registry_util::RegistryOverrideManager registry_overrides_;
};

}  // namespace

// Verify that we return success on no registration (which means stable),
// whether per-system or per-user install.
TEST_F(GoogleUpdateSettingsTest, CurrentChromeChannelAbsent) {
  // Per-system first.
  base::string16 channel;
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
  base::string16 channel;
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
  base::string16 channel;
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(true,
                                                                 &channel));
  EXPECT_STREQ(L"", channel.c_str());

  // Per-user lookup should succeed.
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(false,
                                                                 &channel));
  EXPECT_STREQ(L"", channel.c_str());
}

// Test that the channel is pulled from the binaries for multi-install products.
TEST_F(GoogleUpdateSettingsTest, MultiInstallChannelFromBinaries) {
  SetMultiApField(USER_INSTALL, L"2.0-dev-multi-chrome");
  base::string16 channel;

  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(false,
                                                                 &channel));
  EXPECT_STREQ(L"dev-m", channel.c_str());

  // See if the same happens if the product's ap is cleared.
  SetApField(USER_INSTALL, L"");
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(false,
                                                                 &channel));
  EXPECT_STREQ(L"dev-m", channel.c_str());

  // Test the converse (binaries are stable, Chrome is other).
  SetMultiApField(USER_INSTALL, L"-multi-chrome");
  SetApField(USER_INSTALL, L"2.0-dev-multi-chrome");
  EXPECT_TRUE(GoogleUpdateSettings::GetChromeChannelAndModifiers(false,
                                                                 &channel));
  EXPECT_STREQ(L"m", channel.c_str());
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
  static_assert(arraysize(full) == arraysize(plain), "bad full array size");
  const wchar_t* const multifail[] = {
    L"-multifail",
    L"1.1-multifail",
    L"1.1-dev-multifail"
  };
  static_assert(arraysize(multifail) == arraysize(plain),
                "bad multifail array size");
  const wchar_t* const multifail_full[] = {
    L"-multifail-full",
    L"1.1-multifail-full",
    L"1.1-dev-multifail-full"
  };
  static_assert(arraysize(multifail_full) == arraysize(plain),
                "bad multifail_full array size");
  const wchar_t* const* input_arrays[] = {
    plain,
    full,
    multifail,
    multifail_full
  };
  ChannelInfo v;
  for (const installer::ArchiveType archive_type : archive_types) {
    for (const int result : results) {
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

      for (const wchar_t* const* inputs : input_arrays) {
        if (archive_type == installer::UNKNOWN_ARCHIVE_TYPE) {
          // "-full" is untouched if the archive type is unknown.
          // "-multifail" is unconditionally removed.
          if (inputs == full || inputs == multifail_full)
            outputs = full;
          else
            outputs = plain;
        }
        for (size_t input_idx = 0; input_idx < arraysize(plain); ++input_idx) {
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
  base::string16 ap_key_value = ReadApKeyValue();
  base::string16 reg_key = GetApKeyPath();
  HKEY reg_root = HKEY_CURRENT_USER;
  bool ap_key_deleted = false;
  RegKey key;
  if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(), KEY_ALL_ACCESS) !=
      ERROR_SUCCESS) {
    work_item_list->AddCreateRegKeyWorkItem(
        reg_root, reg_key, WorkItem::kWow64Default);
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
            RegKey().Open(HKEY_LOCAL_MACHINE,
                          GoogleUpdateSettings::kPoliciesKey,
                          KEY_QUERY_VALUE));
  bool is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::kDefaultUpdatePolicy,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  // The policy key exists, but there are no values of interest present.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey().Create(HKEY_LOCAL_MACHINE,
                            GoogleUpdateSettings::kPoliciesKey,
                            KEY_SET_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey().Open(HKEY_LOCAL_MACHINE,
                          GoogleUpdateSettings::kPoliciesKey,
                          KEY_QUERY_VALUE));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::kDefaultUpdatePolicy,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);
}

TEST_F(GoogleUpdateSettingsTest, UpdateProfileCountsSystemInstall) {
  // Override FILE_MODULE and FILE_EXE with a path somewhere in the default
  // system-level install location so that
  // GoogleUpdateSettings::IsSystemInstall() returns true.
  base::FilePath file_exe;
  ASSERT_TRUE(PathService::Get(base::FILE_EXE, &file_exe));
  base::FilePath install_dir(installer::GetChromeInstallPath(
      true /* system_install */, BrowserDistribution::GetDistribution()));
  file_exe = install_dir.Append(file_exe.BaseName());
  base::ScopedPathOverride file_module_override(
      base::FILE_MODULE, file_exe, true /* is_absolute */, false /* create */);
  base::ScopedPathOverride file_exe_override(
      base::FILE_EXE, file_exe, true /* is_absolute */, false /* create */);

  // No profile count keys present yet.
  const base::string16& state_key = BrowserDistribution::GetDistribution()->
      GetAppRegistrationData().GetStateMediumKey();
  base::string16 num_profiles_path(state_key);
  num_profiles_path.append(L"\\");
  num_profiles_path.append(google_update::kRegProfilesActive);
  base::string16 num_signed_in_path(state_key);
  num_signed_in_path.append(L"\\");
  num_signed_in_path.append(google_update::kRegProfilesSignedIn);

  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            RegKey().Open(HKEY_LOCAL_MACHINE,
                          num_profiles_path.c_str(),
                          KEY_QUERY_VALUE));
  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            RegKey().Open(HKEY_LOCAL_MACHINE,
                          num_signed_in_path.c_str(),
                          KEY_QUERY_VALUE));

  // Show time! Write the values.
  GoogleUpdateSettings::UpdateProfileCounts(3, 2);

  // Verify the keys were created.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey().Open(HKEY_LOCAL_MACHINE,
                          num_profiles_path.c_str(),
                          KEY_QUERY_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey().Open(HKEY_LOCAL_MACHINE,
                          num_signed_in_path.c_str(),
                          KEY_QUERY_VALUE));

  base::string16 uniquename;
  EXPECT_TRUE(base::win::GetUserSidString(&uniquename));

  // Verify the values are accessible.
  DWORD num_profiles = 0;
  DWORD num_signed_in = 0;
  base::string16 aggregate;
  EXPECT_EQ(
      ERROR_SUCCESS,
      RegKey(HKEY_LOCAL_MACHINE, num_profiles_path.c_str(),
             KEY_QUERY_VALUE).ReadValueDW(uniquename.c_str(),
                                          &num_profiles));
  EXPECT_EQ(
      ERROR_SUCCESS,
      RegKey(HKEY_LOCAL_MACHINE, num_signed_in_path.c_str(),
             KEY_QUERY_VALUE).ReadValueDW(uniquename.c_str(),
                                          &num_signed_in));
  EXPECT_EQ(
      ERROR_SUCCESS,
      RegKey(HKEY_LOCAL_MACHINE, num_signed_in_path.c_str(),
             KEY_QUERY_VALUE).ReadValue(google_update::kRegAggregateMethod,
                                        &aggregate));

  // Verify the correct values were written.
  EXPECT_EQ(3u, num_profiles);
  EXPECT_EQ(2u, num_signed_in);
  EXPECT_EQ(L"sum()", aggregate);
}

TEST_F(GoogleUpdateSettingsTest, UpdateProfileCountsUserInstall) {
  // Unit tests never operate as an installed application, so will never
  // be a system install.

  // No profile count values present yet.
  const base::string16& state_key = BrowserDistribution::GetDistribution()->
      GetAppRegistrationData().GetStateKey();

  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            RegKey().Open(HKEY_CURRENT_USER,
                          state_key.c_str(),
                          KEY_QUERY_VALUE));

  // Show time! Write the values.
  GoogleUpdateSettings::UpdateProfileCounts(4, 1);

  // Verify the key was created.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey().Open(HKEY_CURRENT_USER,
                          state_key.c_str(),
                          KEY_QUERY_VALUE));

  // Verify the values are accessible.
  base::string16 num_profiles;
  base::string16 num_signed_in;
  EXPECT_EQ(
      ERROR_SUCCESS,
      RegKey(HKEY_CURRENT_USER, state_key.c_str(), KEY_QUERY_VALUE).
          ReadValue(google_update::kRegProfilesActive, &num_profiles));
  EXPECT_EQ(
      ERROR_SUCCESS,
      RegKey(HKEY_CURRENT_USER, state_key.c_str(), KEY_QUERY_VALUE).
          ReadValue(google_update::kRegProfilesSignedIn, &num_signed_in));

  // Verify the correct values were written.
  EXPECT_EQ(L"4", num_profiles);
  EXPECT_EQ(L"1", num_signed_in);
}

#if defined(GOOGLE_CHROME_BUILD)

// Test that the default override is returned if no app-specific override is
// present.
TEST_F(GoogleUpdateSettingsTest, GetAppUpdatePolicyDefaultOverride) {
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(
                       GoogleUpdateSettings::kUpdatePolicyValue,
                       static_cast<DWORD>(0)));
  bool is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(
                       GoogleUpdateSettings::kUpdatePolicyValue,
                       static_cast<DWORD>(1)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(
                       GoogleUpdateSettings::kUpdatePolicyValue,
                       static_cast<DWORD>(2)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::MANUAL_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(
                       GoogleUpdateSettings::kUpdatePolicyValue,
                       static_cast<DWORD>(3)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::AUTO_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  // The default policy should be in force for bogus values.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(
                       GoogleUpdateSettings::kUpdatePolicyValue,
                       static_cast<DWORD>(4)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::kDefaultUpdatePolicy,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);
}

// Test that an app-specific override is used if present.
TEST_F(GoogleUpdateSettingsTest, GetAppUpdatePolicyAppOverride) {
  base::string16 app_policy_value(
      GoogleUpdateSettings::kUpdateOverrideValuePrefix);
  app_policy_value.append(kTestProductGuid);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(
                       GoogleUpdateSettings::kUpdatePolicyValue,
                       static_cast<DWORD>(1)));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(app_policy_value.c_str(),
                                             static_cast<DWORD>(0)));
  bool is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(
                       GoogleUpdateSettings::kUpdatePolicyValue,
                       static_cast<DWORD>(0)));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(app_policy_value.c_str(),
                                             static_cast<DWORD>(1)));
  is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(app_policy_value.c_str(),
                                             static_cast<DWORD>(2)));
  is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::MANUAL_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(app_policy_value.c_str(),
                                             static_cast<DWORD>(3)));
  is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::AUTO_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  // The default policy should be in force for bogus values.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE).WriteValue(app_policy_value.c_str(),
                                             static_cast<DWORD>(4)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED,
            GoogleUpdateSettings::GetAppUpdatePolicy(kTestProductGuid,
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);
}

TEST_F(GoogleUpdateSettingsTest, PerAppUpdatesDisabledByPolicy) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  EXPECT_TRUE(
      SetUpdatePolicyForAppGuid(dist->GetAppGuid(),
                                GoogleUpdateSettings::UPDATES_DISABLED));
  bool is_overridden = false;
  GoogleUpdateSettings::UpdatePolicy update_policy =
      GoogleUpdateSettings::GetAppUpdatePolicy(dist->GetAppGuid(),
                                               &is_overridden);
  EXPECT_TRUE(is_overridden);
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED, update_policy);
  EXPECT_FALSE(GoogleUpdateSettings::AreAutoupdatesEnabled());

  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  update_policy = GoogleUpdateSettings::GetAppUpdatePolicy(dist->GetAppGuid(),
                                                           &is_overridden);
  // Should still have a policy but now that policy should explicitly enable
  // updates.
  EXPECT_TRUE(is_overridden);
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES, update_policy);
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());
}

TEST_F(GoogleUpdateSettingsTest, PerAppUpdatesEnabledWithGlobalDisabled) {
  // Disable updates globally but enable them for Chrome (the app-specific
  // setting should take precedence).
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  BrowserDistribution* binaries = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES);
  EXPECT_TRUE(
      SetUpdatePolicyForAppGuid(dist->GetAppGuid(),
                                GoogleUpdateSettings::AUTOMATIC_UPDATES));
  EXPECT_TRUE(
      SetUpdatePolicyForAppGuid(binaries->GetAppGuid(),
                                GoogleUpdateSettings::AUTOMATIC_UPDATES));
  EXPECT_TRUE(SetGlobalUpdatePolicy(GoogleUpdateSettings::UPDATES_DISABLED));

  // Make sure we read this as still having updates enabled.
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());

  // Make sure that the reset action returns true and is a no-op.
  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES,
            GetUpdatePolicyForAppGuid(dist->GetAppGuid()));
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES,
            GetUpdatePolicyForAppGuid(binaries->GetAppGuid()));
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED, GetGlobalUpdatePolicy());
}

TEST_F(GoogleUpdateSettingsTest, GlobalUpdatesDisabledByPolicy) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  EXPECT_TRUE(SetGlobalUpdatePolicy(GoogleUpdateSettings::UPDATES_DISABLED));
  bool is_overridden = false;

  // The contract for GetAppUpdatePolicy states that |is_overridden| should be
  // set to false when updates are disabled on a non-app-specific basis.
  GoogleUpdateSettings::UpdatePolicy update_policy =
      GoogleUpdateSettings::GetAppUpdatePolicy(dist->GetAppGuid(),
                                               &is_overridden);
  EXPECT_FALSE(is_overridden);
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED, update_policy);
  EXPECT_FALSE(GoogleUpdateSettings::AreAutoupdatesEnabled());

  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  update_policy = GoogleUpdateSettings::GetAppUpdatePolicy(dist->GetAppGuid(),
                                                           &is_overridden);
  // Policy should now be to enable updates, |is_overridden| should still be
  // false.
  EXPECT_FALSE(is_overridden);
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES, update_policy);
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());
}

TEST_F(GoogleUpdateSettingsTest, UpdatesDisabledByTimeout) {
  // Disable updates altogether.
  EXPECT_TRUE(SetUpdateTimeoutOverride(0));
  EXPECT_FALSE(GoogleUpdateSettings::AreAutoupdatesEnabled());
  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());

  // Set the update period to something unreasonable.
  EXPECT_TRUE(SetUpdateTimeoutOverride(
      GoogleUpdateSettings::kCheckPeriodOverrideMinutesMax + 1));
  EXPECT_FALSE(GoogleUpdateSettings::AreAutoupdatesEnabled());
  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());
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

  void SetUp() override {
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
  EXPECT_EQ(base::string16(),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
}

// Tests that GetUninstallCommandLine returns an empty string if there's no
// UninstallCmdLine value in the Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestNoValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE);
  EXPECT_EQ(base::string16(),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
}

// Tests that GetUninstallCommandLine returns an empty string if there's an
// empty UninstallCmdLine value in the Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestEmptyValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
    .WriteValue(google_update::kRegUninstallCmdLine, L"");
  EXPECT_EQ(base::string16(),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
}

// Tests that GetUninstallCommandLine returns the correct string if there's an
// UninstallCmdLine value in the Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestRealValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
      .WriteValue(google_update::kRegUninstallCmdLine, kDummyCommand);
  EXPECT_EQ(base::string16(kDummyCommand),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
  // Make sure that there's no value in the other level (user or system).
  EXPECT_EQ(base::string16(),
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

  void SetUp() override {
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
  base::Version expected(base::UTF16ToUTF8(kDummyVersion));
  EXPECT_EQ(expected,
      GoogleUpdateSettings::GetGoogleUpdateVersion(system_install_));
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

const StatsState::UserLevelState StatsState::kUserLevel = {};
const StatsState::SystemLevelState StatsState::kSystemLevel = {};

// A value parameterized test for testing the stats collection consent setting.
class CollectStatsConsent : public ::testing::TestWithParam<StatsState> {
 public:
  static void SetUpTestCase();
  static void TearDownTestCase();
 protected:
  void SetUp() override;
  static void MakeChromeMultiInstall(HKEY root_key);
  static void ApplySetting(StatsState::StateSetting setting,
                           HKEY root_key,
                           const base::string16& reg_key);

  static base::string16* chrome_version_key_;
  static base::string16* chrome_state_key_;
  static base::string16* chrome_state_medium_key_;
  static base::string16* binaries_state_key_;
  static base::string16* binaries_state_medium_key_;
  registry_util::RegistryOverrideManager override_manager_;
};

base::string16* CollectStatsConsent::chrome_version_key_;
base::string16* CollectStatsConsent::chrome_state_key_;
base::string16* CollectStatsConsent::chrome_state_medium_key_;
base::string16* CollectStatsConsent::binaries_state_key_;
base::string16* CollectStatsConsent::binaries_state_medium_key_;

void CollectStatsConsent::SetUpTestCase() {
  BrowserDistribution* dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER);
  chrome_version_key_ = new base::string16(dist->GetVersionKey());
  chrome_state_key_ = new base::string16(dist->GetStateKey());
  chrome_state_medium_key_ = new base::string16(dist->GetStateMediumKey());

  dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES);
  binaries_state_key_ = new base::string16(dist->GetStateKey());
  binaries_state_medium_key_ = new base::string16(dist->GetStateMediumKey());
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
  base::string16 reg_temp_name(
      stats_state.system_level() ? L"HKLM_" : L"HKCU_");
  reg_temp_name += L"CollectStatsConsent";
  override_manager_.OverrideRegistry(root_key);

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
                                       const base::string16& reg_key) {
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
  const base::string16* const reg_keys[] = {
    chrome_state_key_,
    chrome_state_medium_key_,
    binaries_state_key_,
    binaries_state_medium_key_,
  };
  int key_index = ((GetParam().system_level() ? 1 : 0) +
                   (GetParam().multi_install() ? 2 : 0));
  const base::string16& reg_key = *reg_keys[key_index];
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

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <windows.h>
#include <versionhelpers.h>  // windows.h must be before.

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "chrome_elf/chrome_elf_security.h"
#include "chrome_elf/nt_registry/nt_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using namespace install_static;

namespace {

const wchar_t kCanaryExePath[] =
    L"C:\\Users\\user\\AppData\\Local\\Google\\Chrome SxS\\Application"
    L"\\chrome.exe";
const wchar_t kChromeSystemExePath[] =
    L"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe";
const wchar_t kChromeUserExePath[] =
    L"C:\\Users\\user\\AppData\\Local\\Google\\Chrome\\Application\\chrome.exe";
const wchar_t kChromiumExePath[] =
    L"C:\\Users\\user\\AppData\\Local\\Chromium\\Application\\chrome.exe";

bool SetSecurityFinchFlag(bool creation) {
  bool success = true;
  base::win::RegKey security_key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);

  if (creation) {
    if (ERROR_SUCCESS !=
        security_key.CreateKey(elf_sec::kRegSecurityFinchPath, KEY_QUERY_VALUE))
      success = false;
  } else {
    if (ERROR_SUCCESS != security_key.DeleteKey(elf_sec::kRegSecurityFinchPath))
      success = false;
  }

  security_key.Close();
  return success;
}

bool IsSecuritySet() {
  typedef decltype(GetProcessMitigationPolicy)* GetProcessMitigationPolicyFunc;

  // Check the settings from EarlyBrowserSecurity().
  if (::IsWindows8OrGreater()) {
    GetProcessMitigationPolicyFunc get_process_mitigation_policy =
        reinterpret_cast<GetProcessMitigationPolicyFunc>(::GetProcAddress(
            ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
    if (!get_process_mitigation_policy)
      return false;

    // Check that extension points are disabled.
    // (Legacy hooking.)
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
    if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                       ProcessExtensionPointDisablePolicy,
                                       &policy, sizeof(policy)))
      return false;

    return policy.DisableExtensionPoints;
  }

  return true;
}

void RegRedirect(nt::ROOT_KEY key,
                 registry_util::RegistryOverrideManager& rom) {
  base::string16 temp;

  if (key == nt::HKCU) {
    rom.OverrideRegistry(HKEY_CURRENT_USER, &temp);
    ::wcsncpy(nt::HKCU_override, temp.c_str(), nt::g_kRegMaxPathLen - 1);
  } else if (key == nt::HKLM) {
    rom.OverrideRegistry(HKEY_LOCAL_MACHINE, &temp);
    ::wcsncpy(nt::HKLM_override, temp.c_str(), nt::g_kRegMaxPathLen - 1);
  }
  // nt::AUTO should not be passed into this function.
}

TEST(ChromeElfUtilTest, CanaryTest) {
  EXPECT_TRUE(IsSxSChrome(kCanaryExePath));
  EXPECT_FALSE(IsSxSChrome(kChromeUserExePath));
  EXPECT_FALSE(IsSxSChrome(kChromiumExePath));
}

TEST(ChromeElfUtilTest, SystemInstallTest) {
  EXPECT_TRUE(IsSystemInstall(kChromeSystemExePath));
  EXPECT_FALSE(IsSystemInstall(kChromeUserExePath));
}

TEST(ChromeElfUtilTest, BrowserProcessTest) {
  EXPECT_EQ(ProcessType::UNINITIALIZED, g_process_type);
  InitializeProcessType();
  EXPECT_FALSE(IsNonBrowserProcess());
}

TEST(ChromeElfUtilTest, BrowserProcessSecurityTest) {
  if (!::IsWindows8OrGreater())
    return;

  // Set up registry override for this test.
  registry_util::RegistryOverrideManager override_manager;
  RegRedirect(nt::HKCU, override_manager);

  // First, ensure that the emergency-off finch signal works.
  EXPECT_TRUE(SetSecurityFinchFlag(true));
  elf_security::EarlyBrowserSecurity();
  EXPECT_FALSE(IsSecuritySet());
  EXPECT_TRUE(SetSecurityFinchFlag(false));

  // Second, test that the process mitigation is set when no finch signal.
  elf_security::EarlyBrowserSecurity();
  EXPECT_TRUE(IsSecuritySet());
}

//------------------------------------------------------------------------------
// NT registry API tests (chrome_elf_reg)
//------------------------------------------------------------------------------

TEST(ChromeElfUtilTest, NTRegistry) {
  HANDLE key_handle;
  const wchar_t* dword_val_name = L"DwordTestValue";
  DWORD dword_val = 1234;
  const wchar_t* sz_val_name = L"SzTestValue";
  base::string16 sz_val = L"blah de blah de blahhhhh.";
  const wchar_t* sz_val_name2 = L"SzTestValueEmpty";
  base::string16 sz_val2 = L"";
  const wchar_t* multisz_val_name = L"SzmultiTestValue";
  std::vector<base::string16> multisz_val;
  base::string16 multi1 = L"one";
  base::string16 multi2 = L"two";
  base::string16 multi3 = L"three";
  const wchar_t* multisz_val_name2 = L"SzmultiTestValueBad";
  base::string16 multi_empty = L"";
  const wchar_t* sz_new_key_1 = L"test\\new\\subkey";
  const wchar_t* sz_new_key_2 = L"test\\new\\subkey\\blah\\";
  const wchar_t* sz_new_key_3 = L"\\test\\new\\subkey\\\\blah2";

  // Set up registry override for this test.
  registry_util::RegistryOverrideManager override_manager;
  RegRedirect(nt::HKCU, override_manager);

  // Create a temp key to play under.
  ASSERT_TRUE(nt::CreateRegKey(nt::HKCU, elf_sec::kRegSecurityPath,
                               KEY_ALL_ACCESS, &key_handle));

  // Exercise the supported getter & setter functions.
  EXPECT_TRUE(nt::SetRegValueDWORD(key_handle, dword_val_name, dword_val));
  EXPECT_TRUE(nt::SetRegValueSZ(key_handle, sz_val_name, sz_val));
  EXPECT_TRUE(nt::SetRegValueSZ(key_handle, sz_val_name2, sz_val2));

  DWORD get_dword = 0;
  base::string16 get_sz;
  EXPECT_TRUE(nt::QueryRegValueDWORD(key_handle, dword_val_name, &get_dword) &&
              get_dword == dword_val);
  EXPECT_TRUE(nt::QueryRegValueSZ(key_handle, sz_val_name, &get_sz) &&
              get_sz.compare(sz_val) == 0);
  EXPECT_TRUE(nt::QueryRegValueSZ(key_handle, sz_val_name2, &get_sz) &&
              get_sz.compare(sz_val2) == 0);

  multisz_val.push_back(multi1);
  multisz_val.push_back(multi2);
  multisz_val.push_back(multi3);
  EXPECT_TRUE(
      nt::SetRegValueMULTISZ(key_handle, multisz_val_name, multisz_val));
  multisz_val.clear();
  multisz_val.push_back(multi_empty);
  EXPECT_TRUE(
      nt::SetRegValueMULTISZ(key_handle, multisz_val_name2, multisz_val));
  multisz_val.clear();

  EXPECT_TRUE(
      nt::QueryRegValueMULTISZ(key_handle, multisz_val_name, &multisz_val));
  if (multisz_val.size() == 3) {
    EXPECT_TRUE(multi1.compare(multisz_val.at(0)) == 0);
    EXPECT_TRUE(multi2.compare(multisz_val.at(1)) == 0);
    EXPECT_TRUE(multi3.compare(multisz_val.at(2)) == 0);
  } else {
    EXPECT_TRUE(false);
  }
  multisz_val.clear();

  EXPECT_TRUE(
      nt::QueryRegValueMULTISZ(key_handle, multisz_val_name2, &multisz_val));
  if (multisz_val.size() == 1) {
    EXPECT_TRUE(multi_empty.compare(multisz_val.at(0)) == 0);
  } else {
    EXPECT_TRUE(false);
  }
  multisz_val.clear();

  // Clean up
  EXPECT_TRUE(nt::DeleteRegKey(key_handle));
  nt::CloseRegKey(key_handle);

  // More tests for CreateRegKey recursion.
  ASSERT_TRUE(
      nt::CreateRegKey(nt::HKCU, sz_new_key_1, KEY_ALL_ACCESS, nullptr));
  EXPECT_TRUE(nt::OpenRegKey(nt::HKCU, sz_new_key_1, KEY_ALL_ACCESS,
                             &key_handle, nullptr));
  EXPECT_TRUE(nt::DeleteRegKey(key_handle));
  nt::CloseRegKey(key_handle);

  ASSERT_TRUE(
      nt::CreateRegKey(nt::HKCU, sz_new_key_2, KEY_ALL_ACCESS, nullptr));
  EXPECT_TRUE(nt::OpenRegKey(nt::HKCU, sz_new_key_2, KEY_ALL_ACCESS,
                             &key_handle, nullptr));
  EXPECT_TRUE(nt::DeleteRegKey(key_handle));
  nt::CloseRegKey(key_handle);

  ASSERT_TRUE(
      nt::CreateRegKey(nt::HKCU, sz_new_key_3, KEY_ALL_ACCESS, nullptr));
  EXPECT_TRUE(nt::OpenRegKey(nt::HKCU, L"test\\new\\subkey\\blah2",
                             KEY_ALL_ACCESS, &key_handle, nullptr));
  EXPECT_TRUE(nt::DeleteRegKey(key_handle));
  nt::CloseRegKey(key_handle);

  ASSERT_TRUE(nt::CreateRegKey(nt::HKCU, nullptr, KEY_ALL_ACCESS, &key_handle));
  nt::CloseRegKey(key_handle);
}

// Parameterized test with paramters:
// 1: product: "canary" or "google"
// 2: install level: "user" or "system"
// 3: install mode: "single" or "multi"
class ChromeElfUtilTest
    : public testing::TestWithParam<
          std::tuple<const char*, const char*, const char*>> {
 protected:
  void SetUp() override {
    // Set up registry override for these tests.
    RegRedirect(nt::HKLM, override_manager_);
    RegRedirect(nt::HKCU, override_manager_);

    const char* app;
    const char* level;
    const char* mode;
    std::tie(app, level, mode) = GetParam();
    is_canary_ = (std::string(app) == "canary");
    system_level_ = (std::string(level) != "user");
    multi_install_ = (std::string(mode) != "single");
    if (is_canary_) {
      ASSERT_FALSE(system_level_);
      ASSERT_FALSE(multi_install_);
      app_guid_ = kAppGuidCanary;
      chrome_path_ = kCanaryExePath;
    } else {
      app_guid_ = kAppGuidGoogleChrome;
      chrome_path_ =
          (system_level_ ? kChromeSystemExePath : kChromeUserExePath);
    }
    if (multi_install_) {
      SetMultiInstallStateInRegistry(system_level_, true);
      app_guid_ = kAppGuidGoogleBinaries;
    }
  }

  base::string16 BuildKey(const wchar_t* path, const wchar_t* guid) {
    base::string16 full_key_path(path);
    full_key_path.append(1, L'\\');
    full_key_path.append(guid);
    return full_key_path;
  }

  void SetUsageStat(DWORD value, bool state_medium) {
    LONG result = base::win::RegKey(
                      system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                      BuildKey(state_medium ? kRegPathClientStateMedium
                                            : kRegPathClientState,
                               app_guid_)
                          .c_str(),
                      KEY_SET_VALUE)
                      .WriteValue(kRegValueUsageStats, value);
    ASSERT_EQ(ERROR_SUCCESS, result);
  }

  void SetMultiInstallStateInRegistry(bool system_install, bool multi) {
    base::win::RegKey key(
        system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
        BuildKey(kRegPathClientState, kAppGuidGoogleChrome).c_str(),
        KEY_SET_VALUE);
    LONG result;
    if (multi) {
      result = key.WriteValue(kUninstallArgumentsField,
                              L"yadda yadda --multi-install yadda yadda");
    } else {
      result = key.DeleteValue(kUninstallArgumentsField);
    }
    ASSERT_EQ(ERROR_SUCCESS, result);
  }

  void SetChannelName(const base::string16& channel_name) {
    LONG result =
        base::win::RegKey(
            system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
            BuildKey(kRegPathClientState, app_guid_).c_str(), KEY_SET_VALUE)
            .WriteValue(kRegApField, channel_name.c_str());
    ASSERT_EQ(ERROR_SUCCESS, result);
  }

  // This function tests the install_static::GetChromeChannelName function and
  // is based on the ChannelInfoTest.Channels in channel_info_unittest.cc.
  // The |add_modifier| parameter controls whether we expect modifiers in the
  // returned channel name.
  void PerformChannelNameTests(bool add_modifier) {
    // We can't test the channel name correctly for canary mode because the
    // install_static checks whether an exe is a canary executable is based on
    // the path where the exe is running from.
    if (is_canary_)
      return;
    SetChannelName(L"");
    base::string16 channel;
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }

    SetChannelName(L"-full");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }

    SetChannelName(L"1.1-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"beta-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelBeta, channel.c_str());
    }

    SetChannelName(L"1.1-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"beta-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelBeta, channel.c_str());
    }

    SetChannelName(L"1.1-bar");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"beta-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelBeta, channel.c_str());
    }

    SetChannelName(L"1n1-foobar");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"beta-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelBeta, channel.c_str());
    }

    SetChannelName(L"foo-1.1-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }
    SetChannelName(L"2.0-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }

    SetChannelName(L"2.0-dev");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"dev-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelDev, channel.c_str());
    }
    SetChannelName(L"2.0-DEV");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"dev-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelDev, channel.c_str());
    }
    SetChannelName(L"2.0-dev-eloper");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"dev-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelDev, channel.c_str());
    }
    SetChannelName(L"2.0-doom");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"dev-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelDev, channel.c_str());
    }
    SetChannelName(L"250-doom");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"dev-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelDev, channel.c_str());
    }
    SetChannelName(L"bar-2.0-dev");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }
    SetChannelName(L"1.0-dev");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }

    SetChannelName(L"x64-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"beta-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelBeta, channel.c_str());
    }
    SetChannelName(L"bar-x64-beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"beta-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelBeta, channel.c_str());
    }
    SetChannelName(L"x64-Beta");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"beta-m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelBeta, channel.c_str());
    }

    SetChannelName(L"x64-stable");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }
    SetChannelName(L"baz-x64-stable");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }
    SetChannelName(L"x64-Stable");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }

    SetChannelName(L"fuzzy");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }
    SetChannelName(L"foo");
    install_static::GetChromeChannelName(!system_level_, add_modifier,
                                         &channel);
    if (multi_install_ && add_modifier) {
      EXPECT_STREQ(L"m", channel.c_str());
    } else {
      EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str());
    }
    // Variations on stable channel.
    static constexpr const wchar_t* kStableApValues[] = {
        L"-multi-chrome",
        L"x64-stable-multi-chrome",
        L"-stage:ensemble_patching-multi-chrome-full",
        L"-multi-chrome-full",
    };
    for (const wchar_t* ap_value : kStableApValues) {
      SetChannelName(ap_value);
      install_static::GetChromeChannelName(!system_level_, add_modifier,
                                           &channel);
      if (multi_install_ && add_modifier) {
        EXPECT_STREQ(L"m", channel.c_str()) << ap_value;
      } else {
        EXPECT_STREQ(install_static::kChromeChannelStable, channel.c_str())
            << ap_value;
      }
    }
  }

  const wchar_t* app_guid_;
  const wchar_t* chrome_path_;
  bool system_level_;
  bool multi_install_;
  bool is_canary_;
  registry_util::RegistryOverrideManager override_manager_;
};

TEST_P(ChromeElfUtilTest, MultiInstallTest) {
  if (is_canary_)
    return;
  SetMultiInstallStateInRegistry(system_level_, true);
  EXPECT_TRUE(IsMultiInstall(system_level_));

  SetMultiInstallStateInRegistry(system_level_, false);
  EXPECT_FALSE(IsMultiInstall(system_level_));
}

TEST_P(ChromeElfUtilTest, UsageStatsAbsent) {
  EXPECT_FALSE(GetCollectStatsConsentForTesting(chrome_path_));
}

TEST_P(ChromeElfUtilTest, UsageStatsZero) {
  SetUsageStat(0, false);
  EXPECT_FALSE(GetCollectStatsConsentForTesting(chrome_path_));
}

TEST_P(ChromeElfUtilTest, UsageStatsOne) {
  SetUsageStat(1, false);
  EXPECT_TRUE(GetCollectStatsConsentForTesting(chrome_path_));
  if (is_canary_) {
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeUserExePath));
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeSystemExePath));
  } else if (system_level_) {
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kCanaryExePath));
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeUserExePath));
  } else {
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kCanaryExePath));
    EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeSystemExePath));
  }
}

TEST_P(ChromeElfUtilTest, UsageStatsZeroInStateMedium) {
  if (!system_level_)
    return;
  SetUsageStat(0, true);
  EXPECT_FALSE(GetCollectStatsConsentForTesting(chrome_path_));
}

TEST_P(ChromeElfUtilTest, UsageStatsOneInStateMedium) {
  if (!system_level_)
    return;
  SetUsageStat(1, true);
  EXPECT_TRUE(GetCollectStatsConsentForTesting(chrome_path_));
  EXPECT_FALSE(GetCollectStatsConsentForTesting(kCanaryExePath));
  EXPECT_FALSE(GetCollectStatsConsentForTesting(kChromeUserExePath));
}

// TODO(ananta)
// Move this to install_static_unittests.
// http://crbug.com/604923
// This test tests the install_static::GetChromeChannelName function and is
// based on the ChannelInfoTest.Channels in channel_info_unittest.cc
TEST_P(ChromeElfUtilTest, InstallStaticGetChannelNameTest) {
  PerformChannelNameTests(true);   // add_modifier
  PerformChannelNameTests(false);  // !add_modifier
}

INSTANTIATE_TEST_CASE_P(Canary,
                        ChromeElfUtilTest,
                        testing::Combine(testing::Values("canary"),
                                         testing::Values("user"),
                                         testing::Values("single")));
INSTANTIATE_TEST_CASE_P(GoogleChrome,
                        ChromeElfUtilTest,
                        testing::Combine(testing::Values("google"),
                                         testing::Values("user", "system"),
                                         testing::Values("single", "multi")));

}  // namespace

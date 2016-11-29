// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_util.h"

#include <tuple>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/product_install_details.h"
#include "chrome_elf/nt_registry/nt_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace install_static {

// Tests the MatchPattern function in the install_static library.
TEST(InstallStaticTest, MatchPattern) {
  EXPECT_TRUE(MatchPattern(L"", L""));
  EXPECT_TRUE(MatchPattern(L"", L"*"));
  EXPECT_FALSE(MatchPattern(L"", L"*a"));
  EXPECT_FALSE(MatchPattern(L"", L"abc"));
  EXPECT_TRUE(MatchPattern(L"Hello1234", L"He??o*1*"));
  EXPECT_TRUE(MatchPattern(L"Foo", L"F*?"));
  EXPECT_TRUE(MatchPattern(L"Foo", L"F*"));
  EXPECT_FALSE(MatchPattern(L"Foo", L"F*b"));
  EXPECT_TRUE(MatchPattern(L"abcd", L"*c*d"));
  EXPECT_TRUE(MatchPattern(L"abcd", L"*?c*d"));
  EXPECT_FALSE(MatchPattern(L"abcd", L"abcd*efgh"));
  EXPECT_TRUE(MatchPattern(L"foobarabc", L"*bar*"));
}

// Tests the install_static::GetSwitchValueFromCommandLine function.
TEST(InstallStaticTest, GetSwitchValueFromCommandLineTest) {
  // Simple case with one switch.
  std::wstring value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=bar", L"type");
  EXPECT_EQ(L"bar", value);

  // Multiple switches with trailing spaces between them.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar  --abc=def bleh", L"abc");
  EXPECT_EQ(L"def", value);

  // Multiple switches with trailing spaces and tabs between them.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar \t\t\t --abc=def bleh", L"abc");
  EXPECT_EQ(L"def", value);

  // Non existent switch.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --foo=bar  --abc=def bleh", L"type");
  EXPECT_EQ(L"", value);

  // Non existent switch.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe", L"type");
  EXPECT_EQ(L"", value);

  // Non existent switch.
  value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe type=bar", L"type");
  EXPECT_EQ(L"", value);

  // Trailing spaces after the switch.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar      \t\t", L"type");
  EXPECT_EQ(L"bar", value);

  // Multiple switches with trailing spaces and tabs between them.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar      \t\t --foo=bleh", L"foo");
  EXPECT_EQ(L"bleh", value);

  // Nothing after a switch.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=", L"type");
  EXPECT_TRUE(value.empty());

  // Whitespace after a switch.
  value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type= ", L"type");
  EXPECT_TRUE(value.empty());

  // Just tabs after a switch.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=\t\t\t",
                                        L"type");
  EXPECT_TRUE(value.empty());

  // Whitespace after the "=" before the value.
  value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type= bar", L"type");
  EXPECT_EQ(L"bar", value);

  // Tabs after the "=" before the value.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=\t\t\tbar",
                                        L"type");
  EXPECT_EQ(value, L"bar");
}

TEST(InstallStaticTest, BrowserProcessTest) {
  EXPECT_EQ(ProcessType::UNINITIALIZED, g_process_type);
  InitializeProcessType();
  EXPECT_FALSE(IsNonBrowserProcess());
}

class InstallStaticUtilTest
    : public ::testing::TestWithParam<
          std::tuple<InstallConstantIndex, const char*, const char*>> {
 protected:
  InstallStaticUtilTest() {
    InstallConstantIndex mode_index;
    const char* level;
    const char* mode;

    std::tie(mode_index, level, mode) = GetParam();

    mode_ = &kInstallModes[mode_index];
    system_level_ = std::string(level) != "user";
    EXPECT_TRUE(!system_level_ || mode_->supports_system_level);
    multi_install_ = std::string(mode) != "single";
    EXPECT_TRUE(!multi_install_ || mode_->supports_multi_install);
    root_key_ = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    nt_root_key_ = system_level_ ? nt::HKLM : nt::HKCU;

    std::unique_ptr<PrimaryInstallDetails> details =
        base::MakeUnique<PrimaryInstallDetails>();
    details->set_mode(mode_);
    details->set_channel(mode_->default_channel_name);
    details->set_system_level(system_level_);
    details->set_multi_install(multi_install_);
    InstallDetails::SetForProcess(std::move(details));

    base::string16 path;
    override_manager_.OverrideRegistry(root_key_, &path);
    nt::SetTestingOverride(nt_root_key_, path);
  }

  ~InstallStaticUtilTest() {
    InstallDetails::SetForProcess(nullptr);
    nt::SetTestingOverride(nt_root_key_, base::string16());
  }

  bool system_level() const { return system_level_; }

  bool multi_install() const { return multi_install_; }

  const wchar_t* default_channel() const { return mode_->default_channel_name; }

  void SetUsageStat(DWORD value, bool medium) {
    ASSERT_TRUE(!medium || system_level_);
    ASSERT_EQ(ERROR_SUCCESS,
              base::win::RegKey(root_key_, GetUsageStatsKeyPath(medium).c_str(),
                                KEY_SET_VALUE | KEY_WOW64_32KEY)
                  .WriteValue(L"usagestats", value));
  }

  void SetMetricsReportingPolicy(DWORD value) {
#if defined(GOOGLE_CHROME_BUILD)
    static constexpr wchar_t kPolicyKey[] =
        L"Software\\Policies\\Google\\Chrome";
#else
    static constexpr wchar_t kPolicyKey[] = L"Software\\Policies\\Chromium";
#endif

    ASSERT_EQ(ERROR_SUCCESS,
              base::win::RegKey(root_key_, kPolicyKey, KEY_SET_VALUE)
                  .WriteValue(L"MetricsReportingEnabled", value));
  }

 private:
  // Returns the registry path for the key holding the product's usagestats
  // value. |medium| = true returns the path for ClientStateMedium.
  std::wstring GetUsageStatsKeyPath(bool medium) {
    EXPECT_TRUE(!medium || system_level_);

    std::wstring result(L"Software\\");
    if (kUseGoogleUpdateIntegration) {
      result.append(L"Google\\Update\\ClientState");
      if (medium)
        result.append(L"Medium");
      result.push_back(L'\\');
      if (multi_install_)
        result.append(kBinariesAppGuid);
      else
        result.append(mode_->app_guid);
    } else if (multi_install_) {
      result.append(kBinariesPathName);
    } else {
      result.append(kProductPathName);
    }
    return result;
  }

  registry_util::RegistryOverrideManager override_manager_;
  HKEY root_key_ = nullptr;
  nt::ROOT_KEY nt_root_key_ = nt::AUTO;
  const InstallConstants* mode_ = nullptr;
  bool system_level_ = false;
  bool multi_install_ = false;

  DISALLOW_COPY_AND_ASSIGN(InstallStaticUtilTest);
};

TEST_P(InstallStaticUtilTest, UsageStatsAbsent) {
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsZero) {
  SetUsageStat(0, false);
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsZeroMedium) {
  if (!system_level())
    return;
  SetUsageStat(0, true);
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsOne) {
  SetUsageStat(1, false);
  EXPECT_TRUE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsOneMedium) {
  if (!system_level())
    return;
  SetUsageStat(1, true);
  EXPECT_TRUE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, ReportingIsEnforcedByPolicy) {
  bool reporting_enabled = false;
  EXPECT_FALSE(ReportingIsEnforcedByPolicy(&reporting_enabled));

  SetMetricsReportingPolicy(0);
  EXPECT_TRUE(ReportingIsEnforcedByPolicy(&reporting_enabled));
  EXPECT_FALSE(reporting_enabled);

  SetMetricsReportingPolicy(1);
  EXPECT_TRUE(ReportingIsEnforcedByPolicy(&reporting_enabled));
  EXPECT_TRUE(reporting_enabled);
}

TEST_P(InstallStaticUtilTest, UsageStatsPolicy) {
  // Policy alone.
  SetMetricsReportingPolicy(0);
  EXPECT_FALSE(GetCollectStatsConsent());

  SetMetricsReportingPolicy(1);
  EXPECT_TRUE(GetCollectStatsConsent());

  // Policy trumps usagestats.
  SetMetricsReportingPolicy(1);
  SetUsageStat(0, false);
  EXPECT_TRUE(GetCollectStatsConsent());

  SetMetricsReportingPolicy(0);
  SetUsageStat(1, false);
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, GetChromeChannelName) {
  EXPECT_EQ(default_channel(), GetChromeChannelName(false));
  std::wstring expected = default_channel();
  if (multi_install()) {
    if (expected.empty())
      expected = L"m";
    else
      expected += L"-m";
  }
  EXPECT_EQ(expected, GetChromeChannelName(true));
}

#if defined(GOOGLE_CHROME_BUILD)
// Stable supports multi-install at user and system levels.
INSTANTIATE_TEST_CASE_P(Stable,
                        InstallStaticUtilTest,
                        testing::Combine(testing::Values(STABLE_INDEX),
                                         testing::Values("user", "system"),
                                         testing::Values("single", "multi")));
// Canary is single-only at user level.
INSTANTIATE_TEST_CASE_P(Canary,
                        InstallStaticUtilTest,
                        testing::Combine(testing::Values(CANARY_INDEX),
                                         testing::Values("user"),
                                         testing::Values("single")));
#else   // GOOGLE_CHROME_BUILD
// Chromium supports multi-install at user and system levels.
INSTANTIATE_TEST_CASE_P(Chromium,
                        InstallStaticUtilTest,
                        testing::Combine(testing::Values(CHROMIUM_INDEX),
                                         testing::Values("user", "system"),
                                         testing::Values("single", "multi")));
#endif  // !GOOGLE_CHROME_BUILD

}  // namespace install_static

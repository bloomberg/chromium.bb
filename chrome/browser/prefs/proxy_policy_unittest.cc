// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

void assertProxyMode(const ProxyConfigDictionary& dict,
                     ProxyPrefs::ProxyMode expected_mode) {
  ProxyPrefs::ProxyMode actual_mode;
  ASSERT_TRUE(dict.GetMode(&actual_mode));
  EXPECT_EQ(expected_mode, actual_mode);
}

void assertProxyServer(const ProxyConfigDictionary& dict,
                       const std::string& expected) {
  std::string actual;
  if (!expected.empty()) {
    ASSERT_TRUE(dict.GetProxyServer(&actual));
    EXPECT_EQ(expected, actual);
  } else {
    EXPECT_FALSE(dict.GetProxyServer(&actual));
  }
}

void assertPacUrl(const ProxyConfigDictionary& dict,
                  const std::string& expected) {
  std::string actual;
  if (!expected.empty()) {
    ASSERT_TRUE(dict.GetPacUrl(&actual));
    EXPECT_EQ(expected, actual);
  } else {
    EXPECT_FALSE(dict.GetPacUrl(&actual));
  }
}

void assertBypassList(const ProxyConfigDictionary& dict,
                      const std::string& expected) {
  std::string actual;
  if (!expected.empty()) {
    ASSERT_TRUE(dict.GetBypassList(&actual));
    EXPECT_EQ(expected, actual);
  } else {
    EXPECT_FALSE(dict.GetBypassList(&actual));
  }
}

void assertProxyModeWithoutParams(const ProxyConfigDictionary& dict,
                                  ProxyPrefs::ProxyMode proxy_mode) {
  assertProxyMode(dict, proxy_mode);
  assertProxyServer(dict, "");
  assertPacUrl(dict, "");
  assertBypassList(dict, "");
}

TEST(ProxyPolicyTest, OverridesCommandLineOptions) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProxyBypassList, "123");
  command_line.AppendSwitchASCII(switches::kProxyServer, "789");
  scoped_ptr<policy::MockConfigurationPolicyProvider> provider(
      new policy::MockConfigurationPolicyProvider());
  Value* mode_name = Value::CreateStringValue(
      ProxyPrefs::kFixedServersProxyModeName);
  provider->AddPolicy(policy::kPolicyProxyMode, mode_name);
  provider->AddPolicy(policy::kPolicyProxyBypassList,
                      Value::CreateStringValue("abc"));
  provider->AddPolicy(policy::kPolicyProxyServer,
                      Value::CreateStringValue("ghi"));

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  PrefServiceMockBuilder builder;
  builder.WithCommandLine(&command_line);
  scoped_ptr<PrefService> prefs(builder.Create());
  browser::RegisterUserPrefs(prefs.get());
  ProxyConfigDictionary dict(prefs->GetDictionary(prefs::kProxy));
  assertProxyMode(dict, ProxyPrefs::MODE_FIXED_SERVERS);
  assertProxyServer(dict, "789");
  assertPacUrl(dict, "");
  assertBypassList(dict, "123");

  // Try a second time time with the managed PrefStore in place, the
  // manual proxy policy should have removed all traces of the command
  // line and replaced them with the policy versions.
  builder.WithCommandLine(&command_line);
  builder.WithManagedPlatformProvider(provider.get());
  scoped_ptr<PrefService> prefs2(builder.Create());
  browser::RegisterUserPrefs(prefs2.get());
  ProxyConfigDictionary dict2(prefs2->GetDictionary(prefs::kProxy));
  assertProxyMode(dict2, ProxyPrefs::MODE_FIXED_SERVERS);
  assertProxyServer(dict2, "ghi");
  assertPacUrl(dict2, "");
  assertBypassList(dict2, "abc");
}

TEST(ProxyPolicyTest, OverridesUnrelatedCommandLineOptions) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProxyBypassList, "123");
  command_line.AppendSwitchASCII(switches::kProxyServer, "789");
  scoped_ptr<policy::MockConfigurationPolicyProvider> provider(
      new policy::MockConfigurationPolicyProvider());
  Value* mode_name = Value::CreateStringValue(
      ProxyPrefs::kAutoDetectProxyModeName);
  provider->AddPolicy(policy::kPolicyProxyMode, mode_name);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  PrefServiceMockBuilder builder;
  builder.WithCommandLine(&command_line);
  scoped_ptr<PrefService> prefs(builder.Create());
  browser::RegisterUserPrefs(prefs.get());
  ProxyConfigDictionary dict(prefs->GetDictionary(prefs::kProxy));
  assertProxyMode(dict, ProxyPrefs::MODE_FIXED_SERVERS);
  assertProxyServer(dict, "789");
  assertPacUrl(dict, "");
  assertBypassList(dict, "123");

  // Try a second time time with the managed PrefStore in place, the
  // no proxy policy should have removed all traces of the command
  // line proxy settings, even though they were not the specific one
  // set in policy.
  builder.WithCommandLine(&command_line);
  builder.WithManagedPlatformProvider(provider.get());
  scoped_ptr<PrefService> prefs2(builder.Create());
  browser::RegisterUserPrefs(prefs2.get());
  ProxyConfigDictionary dict2(prefs2->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict2, ProxyPrefs::MODE_AUTO_DETECT);
}

TEST(ProxyPolicyTest, OverridesCommandLineNoProxy) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kNoProxyServer);
  scoped_ptr<policy::MockConfigurationPolicyProvider> provider(
      new policy::MockConfigurationPolicyProvider());
  Value* mode_name = Value::CreateStringValue(
      ProxyPrefs::kAutoDetectProxyModeName);
  provider->AddPolicy(policy::kPolicyProxyMode, mode_name);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  PrefServiceMockBuilder builder;
  builder.WithCommandLine(&command_line);
  scoped_ptr<PrefService> prefs(builder.Create());
  browser::RegisterUserPrefs(prefs.get());
  ProxyConfigDictionary dict(prefs->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict, ProxyPrefs::MODE_DIRECT);

  // Try a second time time with the managed PrefStore in place, the
  // auto-detect should be overridden. The default pref store must be
  // in place with the appropriate default value for this to work.
  builder.WithCommandLine(&command_line);
  builder.WithManagedPlatformProvider(provider.get());
  scoped_ptr<PrefService> prefs2(builder.Create());
  browser::RegisterUserPrefs(prefs2.get());
  ProxyConfigDictionary dict2(prefs2->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict2, ProxyPrefs::MODE_AUTO_DETECT);
}

TEST(ProxyPolicyTest, OverridesCommandLineAutoDetect) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kProxyAutoDetect);
  scoped_ptr<policy::MockConfigurationPolicyProvider> provider(
      new policy::MockConfigurationPolicyProvider());
  Value* mode_name = Value::CreateStringValue(
      ProxyPrefs::kDirectProxyModeName);
  provider->AddPolicy(policy::kPolicyProxyMode, mode_name);

  // First verify that the auto-detect is set if there is no managed
  // PrefStore.
  PrefServiceMockBuilder builder;
  builder.WithCommandLine(&command_line);
  scoped_ptr<PrefService> prefs(builder.Create());
  browser::RegisterUserPrefs(prefs.get());
  ProxyConfigDictionary dict(prefs->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict, ProxyPrefs::MODE_AUTO_DETECT);

  // Try a second time time with the managed PrefStore in place, the
  // auto-detect should be overridden. The default pref store must be
  // in place with the appropriate default value for this to work.
  builder.WithCommandLine(&command_line);
  builder.WithManagedPlatformProvider(provider.get());
  scoped_ptr<PrefService> prefs2(builder.Create());
  browser::RegisterUserPrefs(prefs2.get());
  ProxyConfigDictionary dict2(prefs2->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict2, ProxyPrefs::MODE_DIRECT);
}

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/capabilities.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(Switches, Empty) {
  Switches switches;
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_EQ(0u, cmd.GetSwitches().size());
  ASSERT_EQ("", switches.ToString());
}

TEST(Switches, NoValue) {
  Switches switches;
  switches.SetSwitch("hello");

  ASSERT_TRUE(switches.HasSwitch("hello"));
  ASSERT_EQ("", switches.GetSwitchValue("hello"));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_TRUE(cmd.HasSwitch("hello"));
  ASSERT_EQ(FILE_PATH_LITERAL(""), cmd.GetSwitchValueNative("hello"));
  ASSERT_EQ("--hello", switches.ToString());
}

TEST(Switches, Value) {
  Switches switches;
  switches.SetSwitch("hello", "there");

  ASSERT_TRUE(switches.HasSwitch("hello"));
  ASSERT_EQ("there", switches.GetSwitchValue("hello"));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_TRUE(cmd.HasSwitch("hello"));
  ASSERT_EQ(FILE_PATH_LITERAL("there"), cmd.GetSwitchValueNative("hello"));
  ASSERT_EQ("--hello=there", switches.ToString());
}

TEST(Switches, FromOther) {
  Switches switches;
  switches.SetSwitch("a", "1");
  switches.SetSwitch("b", "1");

  Switches switches2;
  switches2.SetSwitch("b", "2");
  switches2.SetSwitch("c", "2");

  switches.SetFromSwitches(switches2);
  ASSERT_EQ("--a=1 --b=2 --c=2", switches.ToString());
}

TEST(Switches, Remove) {
  Switches switches;
  switches.SetSwitch("a", "1");
  switches.RemoveSwitch("a");
  ASSERT_FALSE(switches.HasSwitch("a"));
}

TEST(Switches, Quoting) {
  Switches switches;
  switches.SetSwitch("hello", "a  b");
  switches.SetSwitch("hello2", "  '\"  ");

  ASSERT_EQ("--hello=\"a  b\" --hello2=\"  '\\\"  \"", switches.ToString());
}

TEST(Switches, Multiple) {
  Switches switches;
  switches.SetSwitch("switch");
  switches.SetSwitch("hello", "there");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  switches.AppendToCommandLine(&cmd);
  ASSERT_TRUE(cmd.HasSwitch("switch"));
  ASSERT_TRUE(cmd.HasSwitch("hello"));
  ASSERT_EQ(FILE_PATH_LITERAL("there"), cmd.GetSwitchValueNative("hello"));
  ASSERT_EQ("--hello=there --switch", switches.ToString());
}

TEST(Switches, Unparsed) {
  Switches switches;
  switches.SetUnparsedSwitch("a");
  switches.SetUnparsedSwitch("--b");
  switches.SetUnparsedSwitch("--c=1");
  switches.SetUnparsedSwitch("d=1");
  switches.SetUnparsedSwitch("-e=--1=1");

  ASSERT_EQ("---e=--1=1 --a --b --c=1 --d=1", switches.ToString());
}

TEST(ParseCapabilities, WithAndroidPackage) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("goog:chromeOptions.androidPackage", "abc");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsAndroid());
  ASSERT_EQ("abc", capabilities.android_package);
}

TEST(ParseCapabilities, EmptyAndroidPackage) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("goog:chromeOptions.androidPackage", std::string());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IllegalAndroidPackage) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetInteger("goog:chromeOptions.androidPackage", 123);
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, LogPath) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("goog:chromeOptions.logPath", "path/to/logfile");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_STREQ("path/to/logfile", capabilities.log_path.c_str());
}

TEST(ParseCapabilities, Args) {
  Capabilities capabilities;
  base::ListValue args;
  args.AppendString("arg1");
  args.AppendString("arg2=val");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "args"}, args.Clone());

  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(2u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("arg1"));
  ASSERT_TRUE(capabilities.switches.HasSwitch("arg2"));
  ASSERT_EQ("", capabilities.switches.GetSwitchValue("arg1"));
  ASSERT_EQ("val", capabilities.switches.GetSwitchValue("arg2"));
}

TEST(ParseCapabilities, Prefs) {
  Capabilities capabilities;
  base::DictionaryValue prefs;
  prefs.SetString("key1", "value1");
  prefs.SetString("key2.k", "value2");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "prefs"}, prefs.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.prefs->Equals(&prefs));
}

TEST(ParseCapabilities, LocalState) {
  Capabilities capabilities;
  base::DictionaryValue local_state;
  local_state.SetString("s1", "v1");
  local_state.SetString("s2.s", "v2");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "localState"}, local_state.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.local_state->Equals(&local_state));
}

TEST(ParseCapabilities, Extensions) {
  Capabilities capabilities;
  base::ListValue extensions;
  extensions.AppendString("ext1");
  extensions.AppendString("ext2");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "extensions"}, extensions.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.extensions.size());
  ASSERT_EQ("ext1", capabilities.extensions[0]);
  ASSERT_EQ("ext2", capabilities.extensions[1]);
}

TEST(ParseCapabilities, UnrecognizedProxyType) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "unknown proxy type");
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IllegalProxyType) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetInteger("proxyType", 123);
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, DirectProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "DIRECT");
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("no-proxy-server"));
}

TEST(ParseCapabilities, SystemProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "system");
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(0u, capabilities.switches.GetSize());
}

TEST(ParseCapabilities, PacProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "PAC");
  proxy.SetString("proxyAutoconfigUrl", "test.wpad");
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_EQ("test.wpad", capabilities.switches.GetSwitchValue("proxy-pac-url"));
}

TEST(ParseCapabilities, MissingProxyAutoconfigUrl) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "PAC");
  proxy.SetString("httpProxy", "http://localhost:8001");
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, AutodetectProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "autodetect");
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("proxy-auto-detect"));
}

TEST(ParseCapabilities, ManualProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "manual");
  proxy.SetString("ftpProxy", "localhost:9001");
  proxy.SetString("httpProxy", "localhost:8001");
  proxy.SetString("sslProxy", "localhost:10001");
  proxy.SetString("noProxy", "google.com, youtube.com");
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.switches.GetSize());
  ASSERT_EQ(
      "ftp=localhost:9001;http=localhost:8001;https=localhost:10001",
      capabilities.switches.GetSwitchValue("proxy-server"));
  ASSERT_EQ(
      "google.com, youtube.com",
      capabilities.switches.GetSwitchValue("proxy-bypass-list"));
}

TEST(ParseCapabilities, MissingSettingForManualProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "manual");
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IgnoreNullValueForManualProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "manual");
  proxy.SetString("ftpProxy", "localhost:9001");
  proxy.Set("sslProxy", base::MakeUnique<base::Value>());
  proxy.Set("noProxy", base::MakeUnique<base::Value>());
  base::DictionaryValue caps;
  caps.SetKey("proxy", proxy.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("proxy-server"));
  ASSERT_EQ(
      "ftp=localhost:9001",
      capabilities.switches.GetSwitchValue("proxy-server"));
}

TEST(ParseCapabilities, LoggingPrefsOk) {
  Capabilities capabilities;
  base::DictionaryValue logging_prefs;
  logging_prefs.SetString("Network", "INFO");
  base::DictionaryValue caps;
  caps.SetKey("loggingPrefs", logging_prefs.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.logging_prefs.size());
  ASSERT_EQ(Log::kInfo, capabilities.logging_prefs["Network"]);
}

TEST(ParseCapabilities, LoggingPrefsNotDict) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("loggingPrefs", "INFO");
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsInspectorDomainStatus) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::DictionaryValue logging_prefs;
  logging_prefs.SetString(WebDriverLog::kPerformanceType, "INFO");
  base::DictionaryValue desired_caps;
  desired_caps.SetKey("loggingPrefs", logging_prefs.Clone());
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled,
            capabilities.perf_logging_prefs.network);
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled,
            capabilities.perf_logging_prefs.page);
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultDisabled,
            capabilities.perf_logging_prefs.timeline);
  base::DictionaryValue perf_logging_prefs;
  perf_logging_prefs.SetBoolean("enableNetwork", true);
  perf_logging_prefs.SetBoolean("enablePage", false);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       perf_logging_prefs.Clone());
  Status status = capabilities.Parse(desired_caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyEnabled,
            capabilities.perf_logging_prefs.network);
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyDisabled,
            capabilities.perf_logging_prefs.page);
  ASSERT_EQ(PerfLoggingPrefs::InspectorDomainStatus::kDefaultDisabled,
            capabilities.perf_logging_prefs.timeline);
}

TEST(ParseCapabilities, PerfLoggingPrefsTracing) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::DictionaryValue logging_prefs;
  logging_prefs.SetString(WebDriverLog::kPerformanceType, "INFO");
  base::DictionaryValue desired_caps;
  desired_caps.SetKey("loggingPrefs", logging_prefs.Clone());
  ASSERT_EQ("", capabilities.perf_logging_prefs.trace_categories);
  base::DictionaryValue perf_logging_prefs;
  perf_logging_prefs.SetString("traceCategories", "benchmark,blink.console");
  perf_logging_prefs.SetInteger("bufferUsageReportingInterval", 1234);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       perf_logging_prefs.Clone());
  Status status = capabilities.Parse(desired_caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ("benchmark,blink.console",
            capabilities.perf_logging_prefs.trace_categories);
  ASSERT_EQ(1234,
            capabilities.perf_logging_prefs.buffer_usage_reporting_interval);
}

TEST(ParseCapabilities, PerfLoggingPrefsInvalidInterval) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::DictionaryValue logging_prefs;
  logging_prefs.SetString(WebDriverLog::kPerformanceType, "INFO");
  base::DictionaryValue desired_caps;
  desired_caps.SetKey("loggingPrefs", logging_prefs.Clone());
  base::DictionaryValue perf_logging_prefs;
  // A bufferUsageReportingInterval interval <= 0 will cause DevTools errors.
  perf_logging_prefs.SetInteger("bufferUsageReportingInterval", 0);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       perf_logging_prefs.Clone());
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsNotDict) {
  Capabilities capabilities;
  // Perf log must be enabled if performance log preferences are specified.
  base::DictionaryValue logging_prefs;
  logging_prefs.SetString(WebDriverLog::kPerformanceType, "INFO");
  base::DictionaryValue desired_caps;
  desired_caps.SetKey("loggingPrefs", logging_prefs.Clone());
  desired_caps.SetString("goog:chromeOptions.perfLoggingPrefs",
                         "traceCategories");
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsNoPerfLogLevel) {
  Capabilities capabilities;
  base::DictionaryValue desired_caps;
  base::DictionaryValue perf_logging_prefs;
  perf_logging_prefs.SetBoolean("enableNetwork", true);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       perf_logging_prefs.Clone());
  // Should fail because perf log must be enabled if perf log prefs specified.
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, PerfLoggingPrefsPerfLogOff) {
  Capabilities capabilities;
  base::DictionaryValue logging_prefs;
  // Disable performance log by setting logging level to OFF.
  logging_prefs.SetString(WebDriverLog::kPerformanceType, "OFF");
  base::DictionaryValue desired_caps;
  desired_caps.SetKey("loggingPrefs", logging_prefs.Clone());
  base::DictionaryValue perf_logging_prefs;
  perf_logging_prefs.SetBoolean("enableNetwork", true);
  desired_caps.SetPath({"goog:chromeOptions", "perfLoggingPrefs"},
                       perf_logging_prefs.Clone());
  // Should fail because perf log must be enabled if perf log prefs specified.
  Status status = capabilities.Parse(desired_caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, ExcludeSwitches) {
  Capabilities capabilities;
  base::ListValue exclude_switches;
  exclude_switches.AppendString("switch1");
  exclude_switches.AppendString("switch2");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "excludeSwitches"},
               exclude_switches.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.exclude_switches.size());
  const std::set<std::string>& switches = capabilities.exclude_switches;
  ASSERT_TRUE(switches.find("switch1") != switches.end());
  ASSERT_TRUE(switches.find("switch2") != switches.end());
}

TEST(ParseCapabilities, UseRemoteBrowser) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("goog:chromeOptions.debuggerAddress", "abc:123");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsRemoteBrowser());
  ASSERT_EQ("abc", capabilities.debugger_address.host());
  ASSERT_EQ(123, capabilities.debugger_address.port());
}

TEST(ParseCapabilities, MobileEmulationUserAgent) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.SetString("userAgent", "Agent Smith");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               mobile_emulation.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("user-agent"));
  ASSERT_EQ("Agent Smith", capabilities.switches.GetSwitchValue("user-agent"));
}

TEST(ParseCapabilities, MobileEmulationDeviceMetrics) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.SetInteger("deviceMetrics.width", 360);
  mobile_emulation.SetInteger("deviceMetrics.height", 640);
  mobile_emulation.SetDouble("deviceMetrics.pixelRatio", 3.0);
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               mobile_emulation.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(360, capabilities.device_metrics->width);
  ASSERT_EQ(640, capabilities.device_metrics->height);
  ASSERT_EQ(3.0, capabilities.device_metrics->device_scale_factor);
}

TEST(ParseCapabilities, MobileEmulationDeviceName) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.SetString("deviceName", "Nexus 5");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               mobile_emulation.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());

  ASSERT_EQ(1u, capabilities.switches.GetSize());
  ASSERT_TRUE(capabilities.switches.HasSwitch("user-agent"));
  ASSERT_EQ(
      "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/63.0.3227.0 Mobile "
      "Safari/537.36",
      capabilities.switches.GetSwitchValue("user-agent"));

  ASSERT_EQ(360, capabilities.device_metrics->width);
  ASSERT_EQ(640, capabilities.device_metrics->height);
  ASSERT_EQ(3.0, capabilities.device_metrics->device_scale_factor);
}

TEST(ParseCapabilities, MobileEmulationNotDict) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("goog:chromeOptions.mobileEmulation", "Google Nexus 5");
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, MobileEmulationDeviceMetricsNotDict) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.SetInteger("deviceMetrics", 360);
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               mobile_emulation.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, MobileEmulationDeviceMetricsNotNumbers) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.SetString("deviceMetrics.width", "360");
  mobile_emulation.SetString("deviceMetrics.height", "640");
  mobile_emulation.SetString("deviceMetrics.pixelRatio", "3.0");
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               mobile_emulation.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, MobileEmulationBadDict) {
  Capabilities capabilities;
  base::DictionaryValue mobile_emulation;
  mobile_emulation.SetString("deviceName", "Google Nexus 5");
  mobile_emulation.SetInteger("deviceMetrics.width", 360);
  mobile_emulation.SetInteger("deviceMetrics.height", 640);
  mobile_emulation.SetDouble("deviceMetrics.pixelRatio", 3.0);
  base::DictionaryValue caps;
  caps.SetPath({"goog:chromeOptions", "mobileEmulation"},
               mobile_emulation.Clone());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

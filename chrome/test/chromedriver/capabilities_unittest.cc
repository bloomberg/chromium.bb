// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/capabilities.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ParseCapabilities, WithAndroidPackage) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("chromeOptions.androidPackage", "abc");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.IsAndroid());
  ASSERT_EQ("abc", capabilities.android_package);
}

TEST(ParseCapabilities, EmptyAndroidPackage) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("chromeOptions.androidPackage", std::string());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IllegalAndroidPackage) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetInteger("chromeOptions.androidPackage", 123);
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, LogPath) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("chromeOptions.logPath", "path/to/logfile");
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_STREQ("path/to/logfile", capabilities.log_path.c_str());
}

TEST(ParseCapabilities, NoArgs) {
  Capabilities capabilities;
  base::ListValue args;
  ASSERT_TRUE(args.empty());
  base::DictionaryValue caps;
  caps.Set("chromeOptions.args", args.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.command.GetSwitches().empty());
}

TEST(ParseCapabilities, SingleArgWithoutValue) {
  Capabilities capabilities;
  base::ListValue args;
  args.AppendString("enable-nacl");
  ASSERT_EQ(1u, args.GetSize());
  base::DictionaryValue caps;
  caps.Set("chromeOptions.args", args.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.command.GetSwitches().size());
  ASSERT_TRUE(capabilities.command.HasSwitch("enable-nacl"));
}

TEST(ParseCapabilities, SingleArgWithValue) {
  Capabilities capabilities;
  base::ListValue args;
  args.AppendString("load-extension=/test/extension");
  ASSERT_EQ(1u, args.GetSize());
  base::DictionaryValue caps;
  caps.Set("chromeOptions.args", args.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.command.GetSwitches().size());
  ASSERT_TRUE(capabilities.command.HasSwitch("load-extension"));
  ASSERT_STREQ(
      "/test/extension",
      capabilities.command.GetSwitchValueASCII("load-extension").c_str());
}

TEST(ParseCapabilities, MultipleArgs) {
  Capabilities capabilities;
  base::ListValue args;
  args.AppendString("arg1");
  args.AppendString("arg2=val");
  args.AppendString("arg3='a space'");
  ASSERT_EQ(3u, args.GetSize());
  base::DictionaryValue caps;
  caps.Set("chromeOptions.args", args.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(3u, capabilities.command.GetSwitches().size());
  ASSERT_TRUE(capabilities.command.HasSwitch("arg1"));
  ASSERT_TRUE(capabilities.command.HasSwitch("arg2"));
  ASSERT_STREQ("val", capabilities.command.GetSwitchValueASCII("arg2").c_str());
  ASSERT_TRUE(capabilities.command.HasSwitch("arg3"));
  ASSERT_STREQ("'a space'",
               capabilities.command.GetSwitchValueASCII("arg3").c_str());
}

TEST(ParseCapabilities, Prefs) {
  Capabilities capabilities;
  base::DictionaryValue prefs;
  prefs.SetString("key1", "value1");
  prefs.SetString("key2.k", "value2");
  base::DictionaryValue caps;
  caps.Set("chromeOptions.prefs", prefs.DeepCopy());
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
  caps.Set("chromeOptions.localState", local_state.DeepCopy());
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
  caps.Set("chromeOptions.extensions", extensions.DeepCopy());
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
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IllegalProxyType) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetInteger("proxyType", 123);
  base::DictionaryValue caps;
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, DirectProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "DIRECT");
  base::DictionaryValue caps;
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.command.GetSwitches().size());
  ASSERT_TRUE(capabilities.command.HasSwitch("no-proxy-server"));
}

TEST(ParseCapabilities, SystemProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "system");
  base::DictionaryValue caps;
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.command.GetSwitches().empty());
}

TEST(ParseCapabilities, PacProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "PAC");
  proxy.SetString("proxyAutoconfigUrl", "test.wpad");
  base::DictionaryValue caps;
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.command.GetSwitches().size());
  ASSERT_STREQ(
      "test.wpad",
      capabilities.command.GetSwitchValueASCII("proxy-pac-url").c_str());
}

TEST(ParseCapabilities, MissingProxyAutoconfigUrl) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "PAC");
  proxy.SetString("httpProxy", "http://localhost:8001");
  base::DictionaryValue caps;
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, AutodetectProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "autodetect");
  base::DictionaryValue caps;
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.command.GetSwitches().size());
  ASSERT_TRUE(capabilities.command.HasSwitch("proxy-auto-detect"));
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
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(2u, capabilities.command.GetSwitches().size());
  ASSERT_STREQ(
      "ftp=localhost:9001;http=localhost:8001;https=localhost:10001",
      capabilities.command.GetSwitchValueASCII("proxy-server").c_str());
  ASSERT_STREQ(
      "google.com, youtube.com",
      capabilities.command.GetSwitchValueASCII("proxy-bypass-list").c_str());
}

TEST(ParseCapabilities, MissingSettingForManualProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "manual");
  base::DictionaryValue caps;
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

TEST(ParseCapabilities, IgnoreNullValueForManualProxy) {
  Capabilities capabilities;
  base::DictionaryValue proxy;
  proxy.SetString("proxyType", "manual");
  proxy.SetString("ftpProxy", "localhost:9001");
  proxy.Set("sslProxy", base::Value::CreateNullValue());
  proxy.Set("noProxy", base::Value::CreateNullValue());
  base::DictionaryValue caps;
  caps.Set("proxy", proxy.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, capabilities.command.GetSwitches().size());
  ASSERT_TRUE(capabilities.command.HasSwitch("proxy-server"));
  ASSERT_STREQ(
      "ftp=localhost:9001",
      capabilities.command.GetSwitchValueASCII("proxy-server").c_str());
}

TEST(ParseCapabilities, LoggingPrefsOk) {
  Capabilities capabilities;
  base::DictionaryValue logging_prefs;
  logging_prefs.SetString("Network", "INFO");
  base::DictionaryValue caps;
  caps.Set("loggingPrefs", logging_prefs.DeepCopy());
  Status status = capabilities.Parse(caps);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(capabilities.logging_prefs.get());
  ASSERT_EQ(1u, capabilities.logging_prefs->size());
  std::string log_level;
  ASSERT_TRUE(capabilities.logging_prefs->GetString("Network", &log_level));
  ASSERT_STREQ("INFO", log_level.c_str());
}

TEST(ParseCapabilities, LoggingPrefsNotDict) {
  Capabilities capabilities;
  base::DictionaryValue caps;
  caps.SetString("loggingPrefs", "INFO");
  Status status = capabilities.Parse(caps);
  ASSERT_FALSE(status.IsOk());
}

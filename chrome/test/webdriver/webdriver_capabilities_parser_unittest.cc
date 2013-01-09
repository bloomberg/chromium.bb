// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/zip.h"
#include "chrome/test/webdriver/webdriver_capabilities_parser.h"
#include "chrome/test/webdriver/webdriver_logging.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace webdriver {

TEST(CapabilitiesParser, NoCaps) {
  Capabilities caps;
  DictionaryValue dict;
  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
}

TEST(CapabilitiesParser, SimpleCaps) {
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("chromeOptions", options);

  options->SetString("binary", "binary");
  options->SetString("channel", "channel");
  options->SetBoolean("detach", true);
  options->SetBoolean("loadAsync", true);

  Capabilities caps;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CapabilitiesParser parser(&dict, temp_dir.path(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
  EXPECT_EQ(FILE_PATH_LITERAL("binary"), caps.command.GetProgram().value());
  EXPECT_STREQ("channel", caps.channel.c_str());
  EXPECT_TRUE(caps.detach);
  EXPECT_TRUE(caps.load_async);
}

TEST(CapabilitiesParser, Args) {
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("chromeOptions", options);

  ListValue* args = new ListValue();
  args->Append(Value::CreateStringValue("arg1"));
  args->Append(Value::CreateStringValue("arg2=val"));
  args->Append(Value::CreateStringValue("arg3='a space'"));
  options->Set("args", args);

  Capabilities caps;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CapabilitiesParser parser(&dict, temp_dir.path(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
  EXPECT_TRUE(caps.command.HasSwitch("arg1"));
  EXPECT_STREQ("val", caps.command.GetSwitchValueASCII("arg2").c_str());
  EXPECT_STREQ("'a space'", caps.command.GetSwitchValueASCII("arg3").c_str());
}

TEST(CapabilitiesParser, Extensions) {
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("chromeOptions", options);

  ListValue* extensions = new ListValue();
  extensions->Append(Value::CreateStringValue("TWFu"));
  extensions->Append(Value::CreateStringValue("TWFuTWFu"));
  options->Set("extensions", extensions);

  Capabilities caps;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CapabilitiesParser parser(&dict, temp_dir.path(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
  ASSERT_EQ(2u, caps.extensions.size());
  std::string contents;
  ASSERT_TRUE(file_util::ReadFileToString(caps.extensions[0], &contents));
  EXPECT_STREQ("Man", contents.c_str());
  contents.clear();
  ASSERT_TRUE(file_util::ReadFileToString(caps.extensions[1], &contents));
  EXPECT_STREQ("ManMan", contents.c_str());
}

TEST(CapabilitiesParser, Profile) {
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("chromeOptions", options);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath folder = temp_dir.path().AppendASCII("folder");
  ASSERT_TRUE(file_util::CreateDirectory(folder));
  ASSERT_EQ(4, file_util::WriteFile(
      folder.AppendASCII("data"), "data", 4));
  FilePath zip = temp_dir.path().AppendASCII("data.zip");
  ASSERT_TRUE(zip::Zip(folder, zip, false /* include_hidden_files */));
  std::string contents;
  ASSERT_TRUE(file_util::ReadFileToString(zip, &contents));
  std::string base64;
  ASSERT_TRUE(base::Base64Encode(contents, &base64));
  options->SetString("profile", base64);

  Capabilities caps;
  CapabilitiesParser parser(&dict, temp_dir.path(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
  std::string new_contents;
  ASSERT_TRUE(file_util::ReadFileToString(
      caps.profile.AppendASCII("data"), &new_contents));
  EXPECT_STREQ("data", new_contents.c_str());
}

TEST(CapabilitiesParser, UnknownCap) {
  Capabilities caps;
  DictionaryValue dict;
  dict.SetString("chromeOptions.nosuchcap", "none");
  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_TRUE(parser.Parse());
}

TEST(CapabilitiesParser, ProxyCap) {
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("proxy", options);

  const char kPacUrl[] = "test.wpad";
  options->SetString("proxyType", "PAC");
  options->SetString("proxyAutoconfigUrl", kPacUrl);

  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
  EXPECT_STREQ(kPacUrl,
      caps.command.GetSwitchValueASCII(switches::kProxyPacUrl).c_str());
}

TEST(CapabilitiesParser, ProxyTypeCapIncompatiblePac) {
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("proxy", options);

  options->SetString("proxyType", "pac");
  options->SetString("httpProxy", "http://localhost:8001");

  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_TRUE(parser.Parse());
}

TEST(CapabilitiesParser, ProxyTypeCapIncompatibleManual) {
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("proxy", options);

  options->SetString("proxyType", "manual");

  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_TRUE(parser.Parse());
}

TEST(CapabilitiesParser, ProxyTypeCapNullValue) {
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("proxy", options);

  options->Set("proxyType", Value::CreateNullValue());

  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_TRUE(parser.Parse());
}

TEST(CapabilitiesParser, ProxyTypeManualCap) {
  const char kProxyServers[] = "ftp=localhost:9001;http=localhost:8001";
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("proxy", options);

  options->SetString("proxyType", "manual");
  options->SetString("httpProxy", "localhost:8001");
  options->SetString("ftpProxy", "localhost:9001");

  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
  EXPECT_STREQ(kProxyServers,
      caps.command.GetSwitchValueASCII(switches::kProxyServer).c_str());
}

TEST(CapabilitiesParser, ProxyBypassListCap) {
  const char kBypassList[] = "google.com, youtube.com";
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("proxy", options);

  options->SetString("proxyType", "manual");
  options->SetString("noProxy", kBypassList);

  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
  EXPECT_STREQ(kBypassList,
      caps.command.GetSwitchValueASCII(switches::kProxyBypassList).c_str());
}

TEST(CapabilitiesParser, ProxyBypassListCapNullValue) {
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("proxy", options);

  options->SetString("proxyType", "manual");
  options->Set("noProxy", Value::CreateNullValue());
  options->SetString("httpProxy", "localhost:8001");

  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
  EXPECT_FALSE(caps.command.HasSwitch(switches::kProxyBypassList));
}

TEST(CapabilitiesParser, UnknownProxyCap) {
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("proxy", options);

  options->SetString("proxyType", "DIRECT");
  options->SetString("badProxyCap", "error");

  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
}

TEST(CapabilitiesParser, ProxyFtpServerCapNullValue) {
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("proxy", options);

  options->SetString("proxyType", "manual");
  options->SetString("httpProxy", "localhost:8001");
  options->Set("ftpProxy", Value::CreateNullValue());

  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());
  EXPECT_STREQ("http=localhost:8001",
      caps.command.GetSwitchValueASCII(switches::kProxyServer).c_str());
}

TEST(CapabilitiesParser, DriverLoggingCapString) {
  Capabilities caps;
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("loggingPrefs", options);
  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);

  // A string as the driver logging level works.
  options->SetString("driver", "INFO");
  ASSERT_FALSE(parser.Parse());

  // An integer (here, an enum LogLevel value) doesn't work.
  options->SetInteger("driver", kInfoLogLevel);
  ASSERT_TRUE(parser.Parse());
}

TEST(CapabilitiesParser, ExcludeSwitches) {
  DictionaryValue dict;
  DictionaryValue* options = new DictionaryValue();
  dict.Set("chromeOptions", options);

  ListValue* switches = new ListValue();
  switches->Append(Value::CreateStringValue(switches::kNoFirstRun));
  switches->Append(Value::CreateStringValue(switches::kDisableSync));
  switches->Append(Value::CreateStringValue(switches::kDisableTranslate));
  options->Set("excludeSwitches", switches);

  Capabilities caps;
  CapabilitiesParser parser(&dict, FilePath(), Logger(), &caps);
  ASSERT_FALSE(parser.Parse());

  const std::set<std::string>& rm_set = caps.exclude_switches;
  EXPECT_EQ(static_cast<size_t>(3), rm_set.size());
  ASSERT_TRUE(rm_set.find(switches::kNoFirstRun) != rm_set.end());
  ASSERT_TRUE(rm_set.find(switches::kDisableSync) != rm_set.end());
  ASSERT_TRUE(rm_set.find(switches::kDisableTranslate) != rm_set.end());
}

}  // namespace webdriver

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for helper functions for the Chrome Extensions Proxy Settings API.

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_proxy_api_constants.h"
#include "chrome/browser/extensions/extension_proxy_api_helpers.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_proxy_api_constants;

namespace {

const char kSamplePacScript[] = "test";
const char kSamplePacScriptAsDataUrl[] =
    "data:application/x-ns-proxy-autoconfig;base64,dGVzdA==";
const char kSamplePacScriptUrl[] = "http://wpad/wpad.dat";

// Helper function to create a ProxyServer dictionary as defined in the
// extension API.
DictionaryValue* CreateTestProxyServerDict(const std::string& host) {
  DictionaryValue* dict = new DictionaryValue;
  dict->SetString(keys::kProxyConfigRuleHost, host);
  return dict;
}

// Helper function to create a ProxyServer dictionary as defined in the
// extension API.
DictionaryValue* CreateTestProxyServerDict(const std::string& schema,
                                           const std::string& host,
                                           int port) {
  DictionaryValue* dict = new DictionaryValue;
  dict->SetString(keys::kProxyConfigRuleScheme, schema);
  dict->SetString(keys::kProxyConfigRuleHost, host);
  dict->SetInteger(keys::kProxyConfigRulePort, port);
  return dict;
}

}  // namespace

namespace extension_proxy_api_helpers {

TEST(ExtensionProxyApiHelpers, CreateDataURLFromPACScript) {
  std::string out;
  ASSERT_TRUE(CreateDataURLFromPACScript(kSamplePacScript, &out));
  EXPECT_EQ(kSamplePacScriptAsDataUrl, out);
}

TEST(ExtensionProxyApiHelpers, CreatePACScriptFromDataURL) {
  std::string out;
  ASSERT_TRUE(CreatePACScriptFromDataURL(kSamplePacScriptAsDataUrl, &out));
  EXPECT_EQ(kSamplePacScript, out);

  EXPECT_FALSE(CreatePACScriptFromDataURL("http://www.google.com", &out));
}

TEST(ExtensionProxyApiHelpers, GetProxyModeFromExtensionPref) {
  DictionaryValue proxy_config;
  ProxyPrefs::ProxyMode mode;
  std::string error;

  // Test positive case.
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_DIRECT));
  ASSERT_TRUE(GetProxyModeFromExtensionPref(&proxy_config, &mode, &error));
  EXPECT_EQ(ProxyPrefs::MODE_DIRECT, mode);
  EXPECT_EQ(std::string(), error);

  // Test negative case.
  proxy_config.SetString(keys::kProxyConfigMode, "foobar");
  EXPECT_FALSE(GetProxyModeFromExtensionPref(&proxy_config, &mode, &error));

  // Do not test |error|, as an invalid enumeration value is considered an
  // internal error. It should be filtered by the extensions API.
}

TEST(ExtensionProxyApiHelpers, GetPacUrlFromExtensionPref) {
  std::string out;
  std::string error;

  DictionaryValue proxy_config;
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));

  // Currently we are still missing a PAC script entry.
  // This is silently ignored.
  ASSERT_TRUE(GetPacUrlFromExtensionPref(&proxy_config, &out, &error));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);

  // Set up a pac script.
  DictionaryValue* pacScriptDict = new DictionaryValue;
  pacScriptDict->SetString(keys::kProxyConfigPacScriptUrl, kSamplePacScriptUrl);
  proxy_config.Set(keys::kProxyConfigPacScript, pacScriptDict);

  ASSERT_TRUE(GetPacUrlFromExtensionPref(&proxy_config, &out, &error));
  EXPECT_EQ(kSamplePacScriptUrl, out);
  EXPECT_EQ(std::string(), error);
}

TEST(ExtensionProxyApiHelpers, GetPacDataFromExtensionPref) {
  std::string out;
  std::string error;

  DictionaryValue proxy_config;
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));

  // Currently we are still missing a PAC data entry. This is silently ignored.
  ASSERT_TRUE(GetPacDataFromExtensionPref(&proxy_config, &out, &error));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);

  // Set up a PAC script.
  DictionaryValue* pacScriptDict = new DictionaryValue;
  pacScriptDict->SetString(keys::kProxyConfigPacScriptData, kSamplePacScript);
  proxy_config.Set(keys::kProxyConfigPacScript, pacScriptDict);

  ASSERT_TRUE(GetPacDataFromExtensionPref(&proxy_config, &out, &error));
  EXPECT_EQ(kSamplePacScript, out);
  EXPECT_EQ(std::string(), error);
}

TEST(ExtensionProxyApiHelpers, GetProxyRulesStringFromExtensionPref) {
  std::string out;
  std::string error;

  DictionaryValue proxy_config;
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS));

  // Currently we are still missing a proxy config entry.
  // This is silently ignored.
  ASSERT_TRUE(
      GetProxyRulesStringFromExtensionPref(&proxy_config, &out, &error));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);

  DictionaryValue* proxy_rules = new DictionaryValue;
  proxy_rules->Set(keys::field_name[1], CreateTestProxyServerDict("proxy1"));
  proxy_rules->Set(keys::field_name[2], CreateTestProxyServerDict("proxy2"));
  proxy_config.Set(keys::kProxyConfigRules, proxy_rules);

  ASSERT_TRUE(
      GetProxyRulesStringFromExtensionPref(&proxy_config, &out, &error));
  EXPECT_EQ("http=proxy1:80;https=proxy2:80", out);
  EXPECT_EQ(std::string(), error);
}

TEST(ExtensionProxyApiHelpers, GetBypassListFromExtensionPref) {
  std::string out;
  std::string error;

  DictionaryValue proxy_config;
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS));

  // Currently we are still missing a proxy config entry.
  // This is silently ignored.
  ASSERT_TRUE(
      GetBypassListFromExtensionPref(&proxy_config, &out, &error));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);

  ListValue* bypass_list = new ListValue;
  bypass_list->Append(Value::CreateStringValue("host1"));
  bypass_list->Append(Value::CreateStringValue("host2"));
  DictionaryValue* proxy_rules = new DictionaryValue;
  proxy_rules->Set(keys::kProxyConfigBypassList, bypass_list);
  proxy_config.Set(keys::kProxyConfigRules, proxy_rules);

  ASSERT_TRUE(
      GetBypassListFromExtensionPref(&proxy_config, &out, &error));
  EXPECT_EQ("host1,host2", out);
  EXPECT_EQ(std::string(), error);
}

TEST(ExtensionProxyApiHelpers, CreateProxyConfigDict) {
  std::string error;
  scoped_ptr<DictionaryValue> exp_direct(ProxyConfigDictionary::CreateDirect());
  scoped_ptr<DictionaryValue> out_direct(
      CreateProxyConfigDict(ProxyPrefs::MODE_DIRECT, "", "", "", "", &error));
  EXPECT_TRUE(Value::Equals(exp_direct.get(), out_direct.get()));

  scoped_ptr<DictionaryValue> exp_auto(
      ProxyConfigDictionary::CreateAutoDetect());
  scoped_ptr<DictionaryValue> out_auto(
      CreateProxyConfigDict(ProxyPrefs::MODE_AUTO_DETECT, "", "", "", "",
                            &error));
  EXPECT_TRUE(Value::Equals(exp_auto.get(), out_auto.get()));

  scoped_ptr<DictionaryValue> exp_pac_url(
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptUrl));
  scoped_ptr<DictionaryValue> out_pac_url(
        CreateProxyConfigDict(ProxyPrefs::MODE_PAC_SCRIPT, kSamplePacScriptUrl,
                              "", "", "", &error));
  EXPECT_TRUE(Value::Equals(exp_pac_url.get(), out_pac_url.get()));

  scoped_ptr<DictionaryValue> exp_pac_data(
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptAsDataUrl));
  scoped_ptr<DictionaryValue> out_pac_data(
          CreateProxyConfigDict(ProxyPrefs::MODE_PAC_SCRIPT, "",
                                kSamplePacScript, "", "", &error));
  EXPECT_TRUE(Value::Equals(exp_pac_data.get(), out_pac_data.get()));

  scoped_ptr<DictionaryValue> exp_fixed(
      ProxyConfigDictionary::CreateFixedServers("foo:80", "localhost"));
  scoped_ptr<DictionaryValue> out_fixed(
          CreateProxyConfigDict(ProxyPrefs::MODE_FIXED_SERVERS, "", "",
                                "foo:80", "localhost", &error));
  EXPECT_TRUE(Value::Equals(exp_fixed.get(), out_fixed.get()));

  scoped_ptr<DictionaryValue> exp_system(ProxyConfigDictionary::CreateSystem());
  scoped_ptr<DictionaryValue> out_system(
      CreateProxyConfigDict(ProxyPrefs::MODE_SYSTEM, "", "", "", "", &error));
  EXPECT_TRUE(Value::Equals(exp_system.get(), out_system.get()));

  // Neither of them should have set an error.
  EXPECT_EQ(std::string(), error);
}

TEST(ExtensionProxyApiHelpers, GetProxyServer) {
  DictionaryValue proxy_server_dict;
  net::ProxyServer created;
  std::string error;

  // Test simplest case, no schema nor port specified --> defaults are used.
  proxy_server_dict.SetString(keys::kProxyConfigRuleHost, "proxy_server");
  ASSERT_TRUE(
      GetProxyServer(&proxy_server_dict, net::ProxyServer::SCHEME_HTTP,
                     &created, &error));
  EXPECT_EQ("PROXY proxy_server:80", created.ToPacString());

  // Test complete case.
  proxy_server_dict.SetString(keys::kProxyConfigRuleScheme, "socks4");
  proxy_server_dict.SetInteger(keys::kProxyConfigRulePort, 1234);
  ASSERT_TRUE(
        GetProxyServer(&proxy_server_dict, net::ProxyServer::SCHEME_HTTP,
                       &created, &error));
  EXPECT_EQ("SOCKS proxy_server:1234", created.ToPacString());
}

TEST(ExtensionProxyApiHelpers, JoinUrlList) {
  ListValue list;
  list.Append(Value::CreateStringValue("s1"));
  list.Append(Value::CreateStringValue("s2"));
  list.Append(Value::CreateStringValue("s3"));

  std::string out;
  std::string error;
  ASSERT_TRUE(JoinUrlList(&list, ";", &out, &error));
  EXPECT_EQ("s1;s2;s3", out);
}

// This tests CreateProxyServerDict as well.
TEST(ExtensionProxyApiHelpers, CreateProxyRulesDict) {
  scoped_ptr<DictionaryValue> browser_pref(
      ProxyConfigDictionary::CreateFixedServers(
          "http=proxy1:80;https=proxy2:80;ftp=proxy3:80;socks=proxy4:80",
          "localhost"));
  ProxyConfigDictionary config(browser_pref.get());
  scoped_ptr<DictionaryValue> extension_pref(CreateProxyRulesDict(config));
  ASSERT_TRUE(extension_pref.get());

  scoped_ptr<DictionaryValue> expected(new DictionaryValue);
  expected->Set("proxyForHttp",
                CreateTestProxyServerDict("http", "proxy1", 80));
  expected->Set("proxyForHttps",
                CreateTestProxyServerDict("http", "proxy2", 80));
  expected->Set("proxyForFtp",
                CreateTestProxyServerDict("http", "proxy3", 80));
  expected->Set("fallbackProxy",
                CreateTestProxyServerDict("socks4", "proxy4", 80));
  ListValue* bypass_list = new ListValue;
  bypass_list->Append(Value::CreateStringValue("localhost"));
  expected->Set(keys::kProxyConfigBypassList, bypass_list);

  EXPECT_TRUE(Value::Equals(expected.get(), extension_pref.get()));
}

// Test if a PAC script URL is specified.
TEST(ExtensionProxyApiHelpers, CreatePacScriptDictWithUrl) {
  scoped_ptr<DictionaryValue> browser_pref(
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptUrl));
  ProxyConfigDictionary config(browser_pref.get());
  scoped_ptr<DictionaryValue> extension_pref(CreatePacScriptDict(config));
  ASSERT_TRUE(extension_pref.get());

  scoped_ptr<DictionaryValue> expected(new DictionaryValue);
  expected->SetString(keys::kProxyConfigPacScriptUrl, kSamplePacScriptUrl);

  EXPECT_TRUE(Value::Equals(expected.get(), extension_pref.get()));
}

// Test if a PAC script is encoded in a data URL.
TEST(ExtensionProxyApiHelpers, CreatePacScriptDictWidthData) {
  scoped_ptr<DictionaryValue> browser_pref(
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptAsDataUrl));
  ProxyConfigDictionary config(browser_pref.get());
  scoped_ptr<DictionaryValue> extension_pref(CreatePacScriptDict(config));
  ASSERT_TRUE(extension_pref.get());

  scoped_ptr<DictionaryValue> expected(new DictionaryValue);
  expected->SetString(keys::kProxyConfigPacScriptData, kSamplePacScript);

  EXPECT_TRUE(Value::Equals(expected.get(), extension_pref.get()));
}

TEST(ExtensionProxyApiHelpers, TokenizeToStringList) {
  ListValue expected;
  expected.Append(Value::CreateStringValue("s1"));
  expected.Append(Value::CreateStringValue("s2"));
  expected.Append(Value::CreateStringValue("s3"));

  scoped_ptr<ListValue> out(TokenizeToStringList("s1;s2;s3", ";"));
  EXPECT_TRUE(Value::Equals(&expected, out.get()));
}

}  // namespace extension_proxy_api_helpers

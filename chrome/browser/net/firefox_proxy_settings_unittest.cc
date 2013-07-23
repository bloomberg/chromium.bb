// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/firefox_proxy_settings.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "net/proxy/proxy_config.h"
#include "testing/gtest/include/gtest/gtest.h"

class FirefoxProxySettingsTest : public testing::Test {
};

class TestFirefoxProxySettings : public FirefoxProxySettings {
 public:
  TestFirefoxProxySettings() {}

  static bool TestGetSettingsFromFile(const base::FilePath& pref_file,
                                      FirefoxProxySettings* settings) {
    return GetSettingsFromFile(pref_file, settings);
  }
};

TEST_F(FirefoxProxySettingsTest, TestParse) {
  FirefoxProxySettings settings;

  base::FilePath js_pref_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &js_pref_path));
  js_pref_path = js_pref_path.AppendASCII("firefox3_pref.js");

  EXPECT_TRUE(TestFirefoxProxySettings::TestGetSettingsFromFile(js_pref_path,
                                                                &settings));
  EXPECT_EQ(FirefoxProxySettings::MANUAL, settings.config_type());
  EXPECT_EQ("http_proxy", settings.http_proxy());
  EXPECT_EQ(1111, settings.http_proxy_port());
  EXPECT_EQ("ssl_proxy", settings.ssl_proxy());
  EXPECT_EQ(2222, settings.ssl_proxy_port());
  EXPECT_EQ("ftp_proxy", settings.ftp_proxy());
  EXPECT_EQ(3333, settings.ftp_proxy_port());
  EXPECT_EQ("gopher_proxy", settings.gopher_proxy());
  EXPECT_EQ(4444, settings.gopher_proxy_port());
  EXPECT_EQ("socks_host", settings.socks_host());
  EXPECT_EQ(5555, settings.socks_port());
  EXPECT_EQ(FirefoxProxySettings::V4, settings.socks_version());
  ASSERT_EQ(3U, settings.proxy_bypass_list().size());
  EXPECT_EQ("localhost", settings.proxy_bypass_list()[0]);
  EXPECT_EQ("127.0.0.1", settings.proxy_bypass_list()[1]);
  EXPECT_EQ("noproxy.com", settings.proxy_bypass_list()[2]);
  EXPECT_EQ("", settings.autoconfig_url());

  // Test that ToProxyConfig() properly translates into a net::ProxyConfig.
  net::ProxyConfig config;
  EXPECT_TRUE(settings.ToProxyConfig(&config));

  {
    net::ProxyConfig expected_config;
    expected_config.proxy_rules().ParseFromString("http=http_proxy:1111; "
                                                  "https=ssl_proxy:2222; "
                                                  "ftp=ftp_proxy:3333; "
                                                  "socks=socks_host:5555");
    expected_config.proxy_rules().bypass_rules.ParseFromString(
        "*localhost,  127.0.0.1, *noproxy.com");
  }
}

TEST_F(FirefoxProxySettingsTest, TestParseAutoConfigUrl) {
  FirefoxProxySettings settings;

  base::FilePath js_pref_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &js_pref_path));
  js_pref_path = js_pref_path.AppendASCII("firefox3_pref_pac_url.js");

  EXPECT_TRUE(TestFirefoxProxySettings::TestGetSettingsFromFile(js_pref_path,
                                                                &settings));
  EXPECT_EQ(FirefoxProxySettings::AUTO_FROM_URL, settings.config_type());

  // Everything should be empty except for the autoconfig URL.
  EXPECT_EQ("http://custom-pac-url/", settings.autoconfig_url());
  EXPECT_EQ("", settings.http_proxy());
  EXPECT_EQ(0, settings.http_proxy_port());
  EXPECT_EQ("", settings.ssl_proxy());
  EXPECT_EQ(0, settings.ssl_proxy_port());
  EXPECT_EQ("", settings.ftp_proxy());
  EXPECT_EQ(0, settings.ftp_proxy_port());
  EXPECT_EQ("", settings.gopher_proxy());
  EXPECT_EQ(0, settings.gopher_proxy_port());
  EXPECT_EQ("", settings.socks_host());
  EXPECT_EQ(0, settings.socks_port());
  EXPECT_EQ(0, settings.socks_port());
  EXPECT_EQ(0U, settings.proxy_bypass_list().size());

  // Test that ToProxyConfig() properly translates into a net::ProxyConfig.
  net::ProxyConfig config;
  EXPECT_TRUE(settings.ToProxyConfig(&config));

  EXPECT_TRUE(config.Equals(net::ProxyConfig::CreateFromCustomPacURL(
      GURL("http://custom-pac-url/"))));
}

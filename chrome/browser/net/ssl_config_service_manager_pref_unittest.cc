// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/ssl_config_service_manager.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/ssl_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ListValue;
using base::Value;
using content::BrowserThread;
using net::SSLConfig;
using net::SSLConfigService;

namespace {

void SetCookiePref(TestingProfile* profile, ContentSetting setting) {
  HostContentSettingsMap* host_content_settings_map =
      profile->GetHostContentSettingsMap();
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, setting);
}

}  // namespace

class SSLConfigServiceManagerPrefTest : public testing::Test {
 public:
  SSLConfigServiceManagerPrefTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {}

 protected:
  bool IsChannelIdEnabled(SSLConfigService* config_service) {
    // Pump the message loop to notify the SSLConfigServiceManagerPref that the
    // preferences changed.
    message_loop_.RunAllPending();
    SSLConfig config;
    config_service->GetSSLConfig(&config);
    return config.channel_id_enabled;
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

// Test channel id with no user prefs.
TEST_F(SSLConfigServiceManagerPrefTest, ChannelIDWithoutUserPrefs) {
  TestingPrefService local_state;
  SSLConfigServiceManager::RegisterPrefs(&local_state);
  local_state.SetUserPref(prefs::kEnableOriginBoundCerts,
                          Value::CreateBooleanValue(false));

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(&local_state, NULL));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig config;
  config_service->GetSSLConfig(&config);
  EXPECT_FALSE(config.channel_id_enabled);

  local_state.SetUserPref(prefs::kEnableOriginBoundCerts,
                          Value::CreateBooleanValue(true));
  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  message_loop_.RunAllPending();
  config_service->GetSSLConfig(&config);
  EXPECT_TRUE(config.channel_id_enabled);
}

// Test channel id with user prefs.
TEST_F(SSLConfigServiceManagerPrefTest, ChannelIDWithUserPrefs) {
  TestingPrefService local_state;
  SSLConfigServiceManager::RegisterPrefs(&local_state);
  local_state.SetUserPref(prefs::kEnableOriginBoundCerts,
                          Value::CreateBooleanValue(false));

  TestingProfile testing_profile;
  TestingPrefService* user_prefs = testing_profile.GetTestingPrefService();
  SetCookiePref(&testing_profile, CONTENT_SETTING_BLOCK);
  user_prefs->SetUserPref(prefs::kBlockThirdPartyCookies,
                          Value::CreateBooleanValue(true));

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(&local_state, user_prefs));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  // channelid=false, cookies=block, 3rdpartycookies=block
  EXPECT_FALSE(IsChannelIdEnabled(config_service));

  // channelid=false, cookies=block, 3rdpartycookies=allow
  user_prefs->SetUserPref(prefs::kBlockThirdPartyCookies,
                          Value::CreateBooleanValue(false));
  EXPECT_FALSE(IsChannelIdEnabled(config_service));

  // channelid=false, cookies=allow, 3rdpartycookies=block
  SetCookiePref(&testing_profile, CONTENT_SETTING_ALLOW);
  user_prefs->SetUserPref(prefs::kBlockThirdPartyCookies,
                          Value::CreateBooleanValue(true));
  EXPECT_FALSE(IsChannelIdEnabled(config_service));

  // channelid=false, cookies=allow, 3rdpartycookies=allow
  user_prefs->SetUserPref(prefs::kBlockThirdPartyCookies,
                          Value::CreateBooleanValue(false));
  EXPECT_FALSE(IsChannelIdEnabled(config_service));

  // channelid=true, cookies=block, 3rdpartycookies=block
  local_state.SetUserPref(prefs::kEnableOriginBoundCerts,
                          Value::CreateBooleanValue(true));
  SetCookiePref(&testing_profile, CONTENT_SETTING_BLOCK);
  user_prefs->SetUserPref(prefs::kBlockThirdPartyCookies,
                          Value::CreateBooleanValue(true));
  EXPECT_FALSE(IsChannelIdEnabled(config_service));

  // channelid=true, cookies=block, 3rdpartycookies=allow
  user_prefs->SetUserPref(prefs::kBlockThirdPartyCookies,
                          Value::CreateBooleanValue(false));
  EXPECT_FALSE(IsChannelIdEnabled(config_service));

  // channelid=true, cookies=allow, 3rdpartycookies=block
  SetCookiePref(&testing_profile, CONTENT_SETTING_ALLOW);
  user_prefs->SetUserPref(prefs::kBlockThirdPartyCookies,
                          Value::CreateBooleanValue(true));
  EXPECT_FALSE(IsChannelIdEnabled(config_service));

  // channelid=true, cookies=allow, 3rdpartycookies=allow
  user_prefs->SetUserPref(prefs::kBlockThirdPartyCookies,
                          Value::CreateBooleanValue(false));
  EXPECT_TRUE(IsChannelIdEnabled(config_service));
}

// Test that cipher suites can be disabled. "Good" refers to the fact that
// every value is expected to be successfully parsed into a cipher suite.
TEST_F(SSLConfigServiceManagerPrefTest, GoodDisabledCipherSuites) {
  TestingPrefService local_state;
  SSLConfigServiceManager::RegisterPrefs(&local_state);

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(&local_state, NULL));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  ListValue* list_value = new ListValue();
  list_value->Append(Value::CreateStringValue("0x0004"));
  list_value->Append(Value::CreateStringValue("0x0005"));
  local_state.SetUserPref(prefs::kCipherSuiteBlacklist, list_value);

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  message_loop_.RunAllPending();

  SSLConfig config;
  config_service->GetSSLConfig(&config);

  EXPECT_NE(old_config.disabled_cipher_suites, config.disabled_cipher_suites);
  ASSERT_EQ(2u, config.disabled_cipher_suites.size());
  EXPECT_EQ(0x0004, config.disabled_cipher_suites[0]);
  EXPECT_EQ(0x0005, config.disabled_cipher_suites[1]);
}

// Test that cipher suites can be disabled. "Bad" refers to the fact that
// there are one or more non-cipher suite strings in the preference. They
// should be ignored.
TEST_F(SSLConfigServiceManagerPrefTest, BadDisabledCipherSuites) {
  TestingPrefService local_state;
  SSLConfigServiceManager::RegisterPrefs(&local_state);

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(&local_state, NULL));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  ListValue* list_value = new ListValue();
  list_value->Append(Value::CreateStringValue("0x0004"));
  list_value->Append(Value::CreateStringValue("TLS_NOT_WITH_A_CIPHER_SUITE"));
  list_value->Append(Value::CreateStringValue("0x0005"));
  list_value->Append(Value::CreateStringValue("0xBEEFY"));
  local_state.SetUserPref(prefs::kCipherSuiteBlacklist, list_value);

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  message_loop_.RunAllPending();

  SSLConfig config;
  config_service->GetSSLConfig(&config);

  EXPECT_NE(old_config.disabled_cipher_suites, config.disabled_cipher_suites);
  ASSERT_EQ(2u, config.disabled_cipher_suites.size());
  EXPECT_EQ(0x0004, config.disabled_cipher_suites[0]);
  EXPECT_EQ(0x0005, config.disabled_cipher_suites[1]);
}

// Test that without command-line settings for minimum and maximum SSL
// versions, SSL 3.0 ~ default_version_max() are enabled.
TEST_F(SSLConfigServiceManagerPrefTest, NoCommandLinePrefs) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  PrefServiceMockBuilder builder;
  builder.WithUserPrefs(local_state_store.get());
  scoped_ptr<PrefService> local_state(builder.Create());

  SSLConfigServiceManager::RegisterPrefs(local_state.get());

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(local_state.get(), NULL));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // The default value in the absence of command-line options is that
  // SSL 3.0 ~ default_version_max() are enabled.
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_SSL3, ssl_config.version_min);
  EXPECT_EQ(net::SSLConfigService::default_version_max(),
            ssl_config.version_max);

  // The settings should not be added to the local_state.
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kSSLVersionMin));
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kSSLVersionMax));

  // Explicitly double-check the settings are not in the preference store.
  std::string version_min_str;
  std::string version_max_str;
  EXPECT_FALSE(local_state_store->GetString(prefs::kSSLVersionMin,
                                            &version_min_str));
  EXPECT_FALSE(local_state_store->GetString(prefs::kSSLVersionMax,
                                            &version_max_str));
}

// Test that command-line settings for minimum and maximum SSL versions are
// respected and that they do not persist to the preferences files.
TEST_F(SSLConfigServiceManagerPrefTest, CommandLinePrefs) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kSSLVersionMin, "tls1");
  command_line.AppendSwitchASCII(switches::kSSLVersionMax, "ssl3");

  PrefServiceMockBuilder builder;
  builder.WithUserPrefs(local_state_store.get());
  builder.WithCommandLine(&command_line);
  scoped_ptr<PrefService> local_state(builder.Create());

  SSLConfigServiceManager::RegisterPrefs(local_state.get());

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(local_state.get(), NULL));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // Command-line flags should be respected.
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1, ssl_config.version_min);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_SSL3, ssl_config.version_max);

  // Explicitly double-check the settings are not in the preference store.
  const PrefService::Preference* version_min_pref =
      local_state->FindPreference(prefs::kSSLVersionMin);
  EXPECT_FALSE(version_min_pref->IsUserModifiable());

  const PrefService::Preference* version_max_pref =
      local_state->FindPreference(prefs::kSSLVersionMax);
  EXPECT_FALSE(version_max_pref->IsUserModifiable());

  std::string version_min_str;
  std::string version_max_str;
  EXPECT_FALSE(local_state_store->GetString(prefs::kSSLVersionMin,
                                            &version_min_str));
  EXPECT_FALSE(local_state_store->GetString(prefs::kSSLVersionMax,
                                            &version_max_str));
}

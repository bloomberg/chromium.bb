// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/ssl_config_service_manager.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_store.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/prefs/pref_service_mock_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/ssl/ssl_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ListValue;
using base::Value;
using content::BrowserThread;
using net::SSLConfig;
using net::SSLConfigService;

class SSLConfigServiceManagerPrefTest : public testing::Test {
 public:
  SSLConfigServiceManagerPrefTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {}

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

// Test channel id with no user prefs.
TEST_F(SSLConfigServiceManagerPrefTest, ChannelIDWithoutUserPrefs) {
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(&local_state));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig config;
  config_service->GetSSLConfig(&config);
  EXPECT_TRUE(config.channel_id_enabled);
}

// Test that cipher suites can be disabled. "Good" refers to the fact that
// every value is expected to be successfully parsed into a cipher suite.
TEST_F(SSLConfigServiceManagerPrefTest, GoodDisabledCipherSuites) {
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(&local_state));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  base::ListValue* list_value = new base::ListValue();
  list_value->Append(new base::StringValue("0x0004"));
  list_value->Append(new base::StringValue("0x0005"));
  local_state.SetUserPref(prefs::kCipherSuiteBlacklist, list_value);

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  message_loop_.RunUntilIdle();

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
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(&local_state));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  base::ListValue* list_value = new base::ListValue();
  list_value->Append(new base::StringValue("0x0004"));
  list_value->Append(new base::StringValue("TLS_NOT_WITH_A_CIPHER_SUITE"));
  list_value->Append(new base::StringValue("0x0005"));
  list_value->Append(new base::StringValue("0xBEEFY"));
  local_state.SetUserPref(prefs::kCipherSuiteBlacklist, list_value);

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  message_loop_.RunUntilIdle();

  SSLConfig config;
  config_service->GetSSLConfig(&config);

  EXPECT_NE(old_config.disabled_cipher_suites, config.disabled_cipher_suites);
  ASSERT_EQ(2u, config.disabled_cipher_suites.size());
  EXPECT_EQ(0x0004, config.disabled_cipher_suites[0]);
  EXPECT_EQ(0x0005, config.disabled_cipher_suites[1]);
}

// Test that without command-line settings for minimum and maximum SSL versions,
// SSL 3.0 ~ kDefaultSSLVersionMax are enabled.
TEST_F(SSLConfigServiceManagerPrefTest, NoCommandLinePrefs) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  PrefServiceMockFactory factory;
  factory.set_user_prefs(local_state_store);
  scoped_refptr<PrefRegistrySimple> registry = new PrefRegistrySimple;
  scoped_ptr<PrefService> local_state(factory.Create(registry.get()));

  SSLConfigServiceManager::RegisterPrefs(registry.get());

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(local_state.get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // The default value in the absence of command-line options is that
  // SSL 3.0 ~ kDefaultSSLVersionMax are enabled.
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_SSL3, ssl_config.version_min);
  EXPECT_EQ(net::kDefaultSSLVersionMax, ssl_config.version_max);

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

  PrefServiceMockFactory factory;
  factory.set_user_prefs(local_state_store);
  factory.SetCommandLine(&command_line);
  scoped_refptr<PrefRegistrySimple> registry = new PrefRegistrySimple;
  scoped_ptr<PrefService> local_state(factory.Create(registry.get()));

  SSLConfigServiceManager::RegisterPrefs(registry.get());

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(local_state.get()));
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

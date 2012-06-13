// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/ssl_config_service_manager.h"

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/ssl_config_service.h"
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
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

// Test that cipher suites can be disabled. "Good" refers to the fact that
// every value is expected to be successfully parsed into a cipher suite.
TEST_F(SSLConfigServiceManagerPrefTest, GoodDisabledCipherSuites) {
  TestingPrefService pref_service;
  SSLConfigServiceManager::RegisterPrefs(&pref_service);

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(&pref_service));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  ListValue* list_value = new ListValue();
  list_value->Append(Value::CreateStringValue("0x0004"));
  list_value->Append(Value::CreateStringValue("0x0005"));
  pref_service.SetUserPref(prefs::kCipherSuiteBlacklist, list_value);

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
  TestingPrefService pref_service;
  SSLConfigServiceManager::RegisterPrefs(&pref_service);

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(&pref_service));
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
  pref_service.SetUserPref(prefs::kCipherSuiteBlacklist, list_value);

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

// Test that existing user settings for TLS1.0/SSL3.0 are both ignored and
// cleared from user preferences.
TEST_F(SSLConfigServiceManagerPrefTest, IgnoreLegacySSLSettings) {
  scoped_refptr<TestingPrefStore> user_prefs(new TestingPrefStore());

  PrefServiceMockBuilder builder;
  builder.WithUserPrefs(user_prefs.get());
  scoped_ptr<PrefService> pref_service(builder.Create());

  // SSL3.0 and TLS1.0 used to be user-definable prefs. They are now used as
  // command-line options. Ensure any existing user prefs are ignored in
  // favour of the command-line flags.
  user_prefs->SetBoolean(prefs::kSSL3Enabled, false);
  user_prefs->SetBoolean(prefs::kTLS1Enabled, false);

  // Ensure the preferences exist initially.
  bool is_ssl3_enabled = true;
  EXPECT_TRUE(user_prefs->GetBoolean(prefs::kSSL3Enabled, &is_ssl3_enabled));
  EXPECT_FALSE(is_ssl3_enabled);

  bool is_tls1_enabled = true;
  EXPECT_TRUE(user_prefs->GetBoolean(prefs::kTLS1Enabled, &is_tls1_enabled));
  EXPECT_FALSE(is_tls1_enabled);

  SSLConfigServiceManager::RegisterPrefs(pref_service.get());

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(pref_service.get()));
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

  // The existing user settings should be removed from the pref_service.
  EXPECT_FALSE(pref_service->HasPrefPath(prefs::kSSL3Enabled));
  EXPECT_FALSE(pref_service->HasPrefPath(prefs::kTLS1Enabled));

  // Explicitly double-check the settings are not in the user preference
  // store.
  EXPECT_FALSE(user_prefs->GetBoolean(prefs::kSSL3Enabled, &is_ssl3_enabled));
  EXPECT_FALSE(user_prefs->GetBoolean(prefs::kTLS1Enabled, &is_tls1_enabled));
}

// Test that command-line settings for TLS1.0/SSL3.0 are respected, that they
// disregard any existing user preferences, and that they do not persist to
// the user preferences files.
TEST_F(SSLConfigServiceManagerPrefTest, CommandLineOverridesUserPrefs) {
  scoped_refptr<TestingPrefStore> user_prefs(new TestingPrefStore());

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kDisableSSL3);
  command_line.AppendSwitch(switches::kDisableTLS1);

  PrefServiceMockBuilder builder;
  builder.WithUserPrefs(user_prefs.get());
  builder.WithCommandLine(&command_line);
  scoped_ptr<PrefService> pref_service(builder.Create());

  // Explicitly enable SSL3.0/TLS1.0 in the user preferences, to mirror the
  // more common legacy file.
  user_prefs->SetBoolean(prefs::kSSL3Enabled, true);
  user_prefs->SetBoolean(prefs::kTLS1Enabled, true);

  // Ensure the preferences exist initially.
  bool is_ssl3_enabled = false;
  EXPECT_TRUE(user_prefs->GetBoolean(prefs::kSSL3Enabled, &is_ssl3_enabled));
  EXPECT_TRUE(is_ssl3_enabled);

  bool is_tls1_enabled = false;
  EXPECT_TRUE(user_prefs->GetBoolean(prefs::kTLS1Enabled, &is_tls1_enabled));
  EXPECT_TRUE(is_tls1_enabled);

  SSLConfigServiceManager::RegisterPrefs(pref_service.get());

  scoped_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(pref_service.get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // Command-line flags to disable should override the user preferences to
  // enable.
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1, ssl_config.version_min);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_SSL3, ssl_config.version_max);

  // Explicitly double-check the settings are not in the user preference
  // store.
  const PrefService::Preference* ssl3_enabled_pref =
      pref_service->FindPreference(prefs::kSSL3Enabled);
  EXPECT_FALSE(ssl3_enabled_pref->IsUserModifiable());

  const PrefService::Preference* tls1_enabled_pref =
      pref_service->FindPreference(prefs::kTLS1Enabled);
  EXPECT_FALSE(tls1_enabled_pref->IsUserModifiable());

  EXPECT_FALSE(user_prefs->GetBoolean(prefs::kSSL3Enabled, &is_ssl3_enabled));
  EXPECT_FALSE(user_prefs->GetBoolean(prefs::kTLS1Enabled, &is_tls1_enabled));
}

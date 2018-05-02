// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_params_manager.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ListValue;
using net::SSLConfig;
using net::SSLConfigService;

class SSLConfigServiceManagerPrefTest : public testing::Test {
 public:
  SSLConfigServiceManagerPrefTest() {}

 protected:
  base::MessageLoop message_loop_;
};

// Test channel id with no user prefs.
TEST_F(SSLConfigServiceManagerPrefTest, ChannelIDWithoutUserPrefs) {
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig config;
  config_service->GetSSLConfig(&config);
  EXPECT_FALSE(config.channel_id_enabled);
}

// Test that cipher suites can be disabled. "Good" refers to the fact that
// every value is expected to be successfully parsed into a cipher suite.
TEST_F(SSLConfigServiceManagerPrefTest, GoodDisabledCipherSuites) {
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  auto list_value = std::make_unique<base::ListValue>();
  list_value->AppendString("0x0004");
  list_value->AppendString("0x0005");
  local_state.SetUserPref(prefs::kCipherSuiteBlacklist, std::move(list_value));

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  base::RunLoop().RunUntilIdle();

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

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig old_config;
  config_service->GetSSLConfig(&old_config);
  EXPECT_TRUE(old_config.disabled_cipher_suites.empty());

  auto list_value = std::make_unique<base::ListValue>();
  list_value->AppendString("0x0004");
  list_value->AppendString("TLS_NOT_WITH_A_CIPHER_SUITE");
  list_value->AppendString("0x0005");
  list_value->AppendString("0xBEEFY");
  local_state.SetUserPref(prefs::kCipherSuiteBlacklist, std::move(list_value));

  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  base::RunLoop().RunUntilIdle();

  SSLConfig config;
  config_service->GetSSLConfig(&config);

  EXPECT_NE(old_config.disabled_cipher_suites, config.disabled_cipher_suites);
  ASSERT_EQ(2u, config.disabled_cipher_suites.size());
  EXPECT_EQ(0x0004, config.disabled_cipher_suites[0]);
  EXPECT_EQ(0x0005, config.disabled_cipher_suites[1]);
}

// Test that without command-line settings for minimum and maximum SSL versions,
// TLS versions from 1.0 up to 1.1 or 1.2 are enabled.
TEST_F(SSLConfigServiceManagerPrefTest, NoCommandLinePrefs) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());
  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // In the absence of command-line options, the default TLS version range is
  // enabled.
  EXPECT_EQ(net::kDefaultSSLVersionMin, ssl_config.version_min);
  EXPECT_EQ(net::kDefaultSSLVersionMax, ssl_config.version_max);
  EXPECT_EQ(net::kDefaultTLS13Variant, ssl_config.tls13_variant);

  // The settings should not be added to the local_state.
  EXPECT_FALSE(local_state.HasPrefPath(prefs::kSSLVersionMin));
  EXPECT_FALSE(local_state.HasPrefPath(prefs::kSSLVersionMax));
  EXPECT_FALSE(local_state.HasPrefPath(prefs::kTLS13Variant));

  // Explicitly double-check the settings are not in the preference store.
  std::string version_min_str;
  std::string version_max_str;
  std::string tls13_variant_str;
  EXPECT_FALSE(
      local_state_store->GetString(prefs::kSSLVersionMin, &version_min_str));
  EXPECT_FALSE(
      local_state_store->GetString(prefs::kSSLVersionMax, &version_max_str));
  EXPECT_FALSE(
      local_state_store->GetString(prefs::kTLS13Variant, &tls13_variant_str));
}

// Tests that "ssl3" is not treated as a valid minimum version.
TEST_F(SSLConfigServiceManagerPrefTest, NoSSL3) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  TestingPrefServiceSimple local_state;
  local_state.SetUserPref(prefs::kSSLVersionMin,
                          std::make_unique<base::Value>("ssl3"));
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // The command-line option must not have been honored.
  EXPECT_LE(net::SSL_PROTOCOL_VERSION_TLS1, ssl_config.version_min);
}

// Tests that SSLVersionMin correctly sets the minimum version.
TEST_F(SSLConfigServiceManagerPrefTest, SSLVersionMin) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  TestingPrefServiceSimple local_state;
  local_state.SetUserPref(prefs::kSSLVersionMin,
                          std::make_unique<base::Value>("tls1.1"));
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1_1, ssl_config.version_min);
}

// Tests that SSL max version correctly sets the maximum version.
TEST_F(SSLConfigServiceManagerPrefTest, SSLVersionMax) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  TestingPrefServiceSimple local_state;
  local_state.SetUserPref(prefs::kSSLVersionMax,
                          std::make_unique<base::Value>("tls1.3"));
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1_3, ssl_config.version_max);
}

// Tests that SSL max version can not be set below TLS 1.2.
TEST_F(SSLConfigServiceManagerPrefTest, NoTLS11Max) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  TestingPrefServiceSimple local_state;
  local_state.SetUserPref(prefs::kSSLVersionMax,
                          std::make_unique<base::Value>("tls1.1"));
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // The command-line option must not have been honored.
  EXPECT_LE(net::SSL_PROTOCOL_VERSION_TLS1_2, ssl_config.version_max);
}

// Tests that TLS 1.3 can be disabled via field trials.
TEST_F(SSLConfigServiceManagerPrefTest, TLS13VariantFeatureDisabled) {
  // Toggle the field trial.
  variations::testing::VariationParamsManager variation_params(
      "TLS13Variant", {{"variant", "disabled"}});

  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1_2, ssl_config.version_max);
}

// Tests that Draft23 TLS 1.3 can be enabled via field trials.
TEST_F(SSLConfigServiceManagerPrefTest, TLS13VariantFeatureDraft23) {
  // Toggle the field trial.
  variations::testing::VariationParamsManager variation_params(
      "TLS13Variant", {{"variant", "draft23"}});

  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1_3, ssl_config.version_max);
  EXPECT_EQ(net::kTLS13VariantDraft23, ssl_config.tls13_variant);
}

// Tests that the SSLVersionMax preference overwites the TLS 1.3 variant
// field trial.
TEST_F(SSLConfigServiceManagerPrefTest, TLS13SSLVersionMax) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  // Toggle the field trial.
  variations::testing::VariationParamsManager variation_params(
      "TLS13Variant", {{"variant", "draft23"}});

  TestingPrefServiceSimple local_state;
  local_state.SetUserPref(prefs::kSSLVersionMax,
                          std::make_unique<base::Value>("tls1.2"));
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1_2, ssl_config.version_max);
}

// Tests that disabling TLS 1.3 by preference overwrites the TLS 1.3 field
// trial.
TEST_F(SSLConfigServiceManagerPrefTest, TLS13VariantOverrideDisable) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  // Toggle the field trial.
  variations::testing::VariationParamsManager variation_params(
      "TLS13Variant", {{"variant", "draft23"}});

  TestingPrefServiceSimple local_state;
  local_state.SetUserPref(prefs::kTLS13Variant,
                          std::make_unique<base::Value>("disabled"));
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1_2, ssl_config.version_max);
}

// Tests that enabling TLS 1.3 by preference overwrites the TLS 1.3 field trial.
TEST_F(SSLConfigServiceManagerPrefTest, TLS13VariantOverrideEnable) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  // Toggle the field trial.
  variations::testing::VariationParamsManager variation_params(
      "TLS13Variant", {{"variant", "disabled"}});

  TestingPrefServiceSimple local_state;
  local_state.SetUserPref(prefs::kSSLVersionMax,
                          std::make_unique<base::Value>("tls1.3"));
  local_state.SetUserPref(prefs::kTLS13Variant,
                          std::make_unique<base::Value>("draft23"));
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1_3, ssl_config.version_max);
  EXPECT_EQ(net::kTLS13VariantDraft23, ssl_config.tls13_variant);
}

// Tests that SHA-1 signatures for local trust anchors can be enabled.
TEST_F(SSLConfigServiceManagerPrefTest, SHA1ForLocalAnchors) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager);
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service);

  // By default, SHA-1 local trust anchors should not be enabled when not
  // not using any pref service.
  SSLConfig config1;
  EXPECT_FALSE(config1.sha1_local_anchors_enabled);

  // Using a pref service without any preference set should result in
  // SHA-1 local trust anchors being disabled.
  SSLConfig config2;
  config_service->GetSSLConfig(&config2);
  EXPECT_FALSE(config2.sha1_local_anchors_enabled);

  // Enabling the local preference should result in SHA-1 local trust anchors
  // being enabled.
  local_state.SetUserPref(prefs::kCertEnableSha1LocalAnchors,
                          std::make_unique<base::Value>(true));
  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  base::RunLoop().RunUntilIdle();

  SSLConfig config3;
  config_service->GetSSLConfig(&config3);
  EXPECT_TRUE(config3.sha1_local_anchors_enabled);

  // Disabling the local preference should result in SHA-1 local trust
  // anchors being disabled.
  local_state.SetUserPref(prefs::kCertEnableSha1LocalAnchors,
                          std::make_unique<base::Value>(false));
  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  base::RunLoop().RunUntilIdle();

  SSLConfig config4;
  config_service->GetSSLConfig(&config4);
  EXPECT_FALSE(config4.sha1_local_anchors_enabled);
}

// Tests that Symantec's legacy infrastructure can be enabled.
TEST_F(SSLConfigServiceManagerPrefTest, SymantecLegacyInfrastructure) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  TestingPrefServiceSimple local_state;
  SSLConfigServiceManager::RegisterPrefs(local_state.registry());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          &local_state, base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager);
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service);

  // By default, Symantec's legacy infrastructure should be disabled when
  // not using any pref service.
  SSLConfig config1;
  EXPECT_FALSE(config1.symantec_enforcement_disabled);

  // Using a pref service without any preference set should result in
  // Symantec's legacy infrastructure being disabled.
  SSLConfig config2;
  config_service->GetSSLConfig(&config2);
  EXPECT_FALSE(config2.symantec_enforcement_disabled);

  // Enabling the local preference should result in Symantec's legacy
  // infrastructure being enabled.
  local_state.SetUserPref(prefs::kCertEnableSymantecLegacyInfrastructure,
                          std::make_unique<base::Value>(true));
  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  base::RunLoop().RunUntilIdle();

  SSLConfig config3;
  config_service->GetSSLConfig(&config3);
  EXPECT_TRUE(config3.symantec_enforcement_disabled);

  // Disabling the local preference should result in Symantec's legacy
  // infrastructure being disabled.
  local_state.SetUserPref(prefs::kCertEnableSymantecLegacyInfrastructure,
                          std::make_unique<base::Value>(false));
  // Pump the message loop to notify the SSLConfigServiceManagerPref that the
  // preferences changed.
  base::RunLoop().RunUntilIdle();

  SSLConfig config4;
  config_service->GetSSLConfig(&config4);
  EXPECT_FALSE(config4.symantec_enforcement_disabled);
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/ssl_config/ssl_config_prefs.h"
#include "components/ssl_config/ssl_config_service_manager.h"
#include "components/ssl_config/ssl_config_switches.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::SSLConfig;
using net::SSLConfigService;
using ssl_config::SSLConfigServiceManager;

class CommandLinePrefStoreSSLManagerTest : public testing::Test {
 public:
  CommandLinePrefStoreSSLManagerTest() {}

 protected:
  base::MessageLoop message_loop_;
};

// Test that command-line settings for minimum and maximum SSL versions are
// respected and that they do not persist to the preferences files.
TEST_F(CommandLinePrefStoreSSLManagerTest, CommandLinePrefs) {
  scoped_refptr<TestingPrefStore> local_state_store(new TestingPrefStore());

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kSSLVersionMin, "tls1.1");
  command_line.AppendSwitchASCII(switches::kSSLVersionMax, "tls1.3");

  sync_preferences::PrefServiceMockFactory factory;
  factory.set_user_prefs(local_state_store);
  factory.set_command_line_prefs(new ChromeCommandLinePrefStore(&command_line));
  scoped_refptr<PrefRegistrySimple> registry = new PrefRegistrySimple;
  std::unique_ptr<PrefService> local_state(factory.Create(registry.get()));

  SSLConfigServiceManager::RegisterPrefs(registry.get());

  std::unique_ptr<SSLConfigServiceManager> config_manager(
      SSLConfigServiceManager::CreateDefaultManager(
          local_state.get(), base::ThreadTaskRunnerHandle::Get()));
  ASSERT_TRUE(config_manager.get());
  scoped_refptr<SSLConfigService> config_service(config_manager->Get());
  ASSERT_TRUE(config_service.get());

  SSLConfig ssl_config;
  config_service->GetSSLConfig(&ssl_config);
  // Command-line flags should be respected.
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1_1, ssl_config.version_min);
  EXPECT_EQ(net::SSL_PROTOCOL_VERSION_TLS1_3, ssl_config.version_max);

  // Explicitly double-check the settings are not in the preference store.
  const PrefService::Preference* version_min_pref =
      local_state->FindPreference(ssl_config::prefs::kSSLVersionMin);
  EXPECT_FALSE(version_min_pref->IsUserModifiable());

  const PrefService::Preference* version_max_pref =
      local_state->FindPreference(ssl_config::prefs::kSSLVersionMax);
  EXPECT_FALSE(version_max_pref->IsUserModifiable());

  std::string version_min_str;
  std::string version_max_str;
  EXPECT_FALSE(local_state_store->GetString(ssl_config::prefs::kSSLVersionMin,
                                            &version_min_str));
  EXPECT_FALSE(local_state_store->GetString(ssl_config::prefs::kSSLVersionMax,
                                            &version_max_str));
}

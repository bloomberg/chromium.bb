// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/proxy/proxy_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyConfiguratorTest : public testing::Test {
 public:
  void SetUp() override {
    test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
    manager_.reset(new NetworkPropertiesManager(
        &test_prefs, base::ThreadTaskRunnerHandle::Get()));
    manager_->OnChangeInNetworkID("test");

    test_context_ = DataReductionProxyTestContext::Builder().Build();
    config_.reset(new DataReductionProxyConfigurator(
        test_context_->net_log(), test_context_->event_creator()));
  }

  void TearDown() override {
    // Reset the state of |manager_|.
    manager_->SetIsSecureProxyDisallowedByCarrier(false);
    manager_->SetIsCaptivePortal(false);
    manager_->SetHasWarmupURLProbeFailed(false, false, false);
    manager_->SetHasWarmupURLProbeFailed(false, true, false);
    manager_->SetHasWarmupURLProbeFailed(true, false, false);
    manager_->SetHasWarmupURLProbeFailed(true, true, false);
  }

  std::vector<DataReductionProxyServer> BuildProxyList(
      const std::string& first,
      enum ProxyServer_ProxyType first_proxy_type,
      const std::string& second,
      enum ProxyServer_ProxyType second_proxy_type) {
    std::vector<DataReductionProxyServer> proxies;
    if (!first.empty()) {
      net::ProxyServer proxy =
          net::ProxyServer::FromURI(first, net::ProxyServer::SCHEME_HTTP);
      EXPECT_TRUE(proxy.is_valid()) << first;
      proxies.push_back(DataReductionProxyServer(proxy, first_proxy_type));
    }
    if (!second.empty()) {
      net::ProxyServer proxy =
          net::ProxyServer::FromURI(second, net::ProxyServer::SCHEME_HTTP);
      EXPECT_TRUE(proxy.is_valid()) << second;
      proxies.push_back(DataReductionProxyServer(proxy, second_proxy_type));
    }
    return proxies;
  }

  void CheckProxyConfig(
      const net::ProxyConfig::ProxyRules::Type& expected_rules_type,
      const std::string& expected_http_proxies,
      const std::string& expected_bypass_list) {
    test_context_->RunUntilIdle();
    const net::ProxyConfig::ProxyRules& rules =
        config_->GetProxyConfig().proxy_rules();
    ASSERT_EQ(expected_rules_type, rules.type);
    if (net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME ==
        expected_rules_type) {
      ASSERT_EQ(expected_http_proxies, rules.proxies_for_http.ToPacString());
      ASSERT_EQ(std::string(), rules.proxies_for_https.ToPacString());
      ASSERT_EQ(expected_bypass_list, rules.bypass_rules.ToString());
    }
  }

  void CheckProbeProxyConfig(
      const std::vector<DataReductionProxyServer>& http_proxies,
      bool probe_url_config,
      const net::ProxyConfig::ProxyRules::Type& expected_rules_type,
      const std::string& expected_http_proxies,
      const std::string& expected_bypass_list) {
    test_context_->RunUntilIdle();
    net::ProxyConfig::ProxyRules rules =
        config_
            ->CreateProxyConfig(probe_url_config, *manager_.get(), http_proxies)
            .proxy_rules();
    ASSERT_EQ(expected_rules_type, rules.type);
    if (net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME ==
        expected_rules_type) {
      ASSERT_EQ(expected_http_proxies, rules.proxies_for_http.ToPacString());
      ASSERT_EQ(std::string(), rules.proxies_for_https.ToPacString());
      ASSERT_EQ(expected_bypass_list, rules.bypass_rules.ToString());
    }
  }

  TestingPrefServiceSimple test_prefs;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<DataReductionProxyConfigurator> config_;
  std::unique_ptr<NetworkPropertiesManager> manager_;
};

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestricted) {
  config_->Enable(*manager_,
                  BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedQuic) {
  config_->Enable(*manager_,
                  BuildProxyList("quic://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "QUIC www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedWithBypassRule) {
  config_->SetBypassRules("<local>, *.goo.com");
  config_->Enable(*manager_,
                  BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   "<local>;*.goo.com;");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedWithBypassRuleQuic) {
  config_->SetBypassRules("<local>, *.goo.com");
  config_->Enable(*manager_,
                  BuildProxyList("quic://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "QUIC www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   "<local>;*.goo.com;");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedWithoutFallback) {
  config_->Enable(*manager_,
                  BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                                 std::string(), ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;DIRECT", std::string());
}

TEST_F(DataReductionProxyConfiguratorTest,
       TestUnrestrictedWithoutFallbackQuic) {
  config_->Enable(*manager_,
                  BuildProxyList("quic://www.foo.com:443", ProxyServer::CORE,
                                 std::string(), ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "QUIC www.foo.com:443;DIRECT", std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestSecureRestrictedProxiesAreCore) {
  manager_->SetHasWarmupURLProbeFailed(true, true, true);
  config_->Enable(*manager_,
                  BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "PROXY www.bar.com:80;DIRECT", std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestSecureNonCoreRestricted) {
  manager_->SetHasWarmupURLProbeFailed(true, false, true);
  config_->Enable(*manager_,
                  BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest,
       TestSecureRestrictedProxiesAreNonCore) {
  manager_->SetHasWarmupURLProbeFailed(true, true, true);
  config_->Enable(
      *manager_,
      BuildProxyList("https://www.foo.com:443", ProxyServer::UNSPECIFIED_TYPE,
                     "http://www.bar.com:80", ProxyServer::UNSPECIFIED_TYPE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestInsecureCoreRestricted) {
  manager_->SetHasWarmupURLProbeFailed(false, true, true);
  config_->Enable(*manager_,
                  BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;DIRECT", std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestInsecureNonCoreRestricted) {
  manager_->SetHasWarmupURLProbeFailed(false, false, true);
  config_->Enable(*manager_,
                  BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestSecureInsecureCoreRestricted) {
  manager_->SetHasWarmupURLProbeFailed(true, true, true);
  manager_->SetHasWarmupURLProbeFailed(false, true, true);
  config_->Enable(*manager_,
                  BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME, "",
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestRestrictedQuic) {
  manager_->SetHasWarmupURLProbeFailed(true, true, true);
  config_->Enable(*manager_,
                  BuildProxyList("quic://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "PROXY www.bar.com:80;DIRECT", std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestDisable) {
  config_->Enable(*manager_,
                  BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                                 "http://www.bar.com:80", ProxyServer::CORE));
  config_->Disable();
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_NO_RULES, std::string(),
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestBypassList) {
  config_->SetBypassRules("http://www.google.com, fefe:13::abc/33");

  net::ProxyBypassRules expected;
  expected.AddRuleFromString("http://www.google.com");
  expected.AddRuleFromString("fefe:13::abc/33");

  EXPECT_TRUE(expected.Equals(config_->bypass_rules_));
}

TEST_F(DataReductionProxyConfiguratorTest,
       TestProbeSecureInsecureCoreRestricted) {
  manager_->SetHasWarmupURLProbeFailed(true, true, true);
  manager_->SetHasWarmupURLProbeFailed(false, true, true);

  const std::vector<DataReductionProxyServer>& http_proxies =
      BuildProxyList("https://www.foo.com:443", ProxyServer::CORE,
                     "http://www.bar.com:80", ProxyServer::CORE);
  config_->Enable(*manager_, http_proxies);
  CheckProbeProxyConfig(http_proxies, true /* probe_url_config */,
                        net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                        "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                        std::string());
  CheckProbeProxyConfig(http_proxies, false /* probe_url_config */,
                        net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME, "",
                        std::string());
}

}  // namespace data_reduction_proxy

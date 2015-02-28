// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "net/base/capturing_net_log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyConfiguratorTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ = new base::TestSimpleTaskRunner();
    net_log_.reset(new net::CapturingNetLog());
    data_reduction_proxy_event_store_.reset(
        new data_reduction_proxy::DataReductionProxyEventStore(task_runner_));
    config_.reset(
        new DataReductionProxyConfigurator(
            task_runner_, net_log_.get(),
            data_reduction_proxy_event_store_.get()));
  }

  void CheckProxyConfig(
      const net::ProxyConfig::ProxyRules::Type& expected_rules_type,
      const std::string& expected_http_proxies,
      const std::string& expected_https_proxies,
      const std::string& expected_bypass_list) {
    task_runner_->RunUntilIdle();
    const net::ProxyConfig::ProxyRules& rules =
        config_->GetProxyConfigOnIOThread().proxy_rules();
    ASSERT_EQ(expected_rules_type, rules.type);
    if (net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME ==
        expected_rules_type) {
      ASSERT_EQ(expected_http_proxies, rules.proxies_for_http.ToPacString());
      ASSERT_EQ(expected_https_proxies, rules.proxies_for_https.ToPacString());
      ASSERT_EQ(expected_bypass_list, rules.bypass_rules.ToString());
    }
  }

  scoped_ptr<DataReductionProxyConfigurator> config_;
  scoped_ptr<net::NetLog> net_log_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_ptr<data_reduction_proxy::DataReductionProxyEventStore>
      data_reduction_proxy_event_store_;
};

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestricted) {
  config_->Enable(false,
                  false,
                  "https://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "");
  CheckProxyConfig(
      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
      "", "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedQuic) {
  config_->Enable(false,
                  false,
                  "quic://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "");
  CheckProxyConfig(
      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      "QUIC www.foo.com:443;PROXY www.bar.com:80;DIRECT",
      "", "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedSSL) {
  config_->Enable(false,
                  false,
                  "https://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "http://www.ssl.com:80/");
  CheckProxyConfig(
      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
      "PROXY www.ssl.com:80;DIRECT",
      "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedSSLQuic) {
  config_->Enable(false,
                  false,
                  "quic://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "http://www.ssl.com:80/");
  CheckProxyConfig(
      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      "QUIC www.foo.com:443;PROXY www.bar.com:80;DIRECT",
      "PROXY www.ssl.com:80;DIRECT",
      "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedWithBypassRule) {
  config_->AddHostPatternToBypass("<local>");
  config_->AddHostPatternToBypass("*.goo.com");
  config_->Enable(false,
                  false,
                  "https://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "");
  CheckProxyConfig(
      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT", "",
      "<local>;*.goo.com;");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedWithBypassRuleQuic) {
  config_->AddHostPatternToBypass("<local>");
  config_->AddHostPatternToBypass("*.goo.com");
  config_->Enable(false,
                  false,
                  "quic://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "");
  CheckProxyConfig(
      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
      "QUIC www.foo.com:443;PROXY www.bar.com:80;DIRECT", "",
      "<local>;*.goo.com;");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedWithoutFallback) {
  config_->Enable(false, false, "https://www.foo.com:443/", "", "");
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;DIRECT", "", "");
}

TEST_F(DataReductionProxyConfiguratorTest,
       TestUnrestrictedWithoutFallbackQuic) {
  config_->Enable(false, false, "quic://www.foo.com:443/", "", "");
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "QUIC www.foo.com:443;DIRECT", "", "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestRestricted) {
  config_->Enable(true,
                  false,
                  "https://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "");
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "PROXY www.bar.com:80;DIRECT", "", "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestRestrictedQuic) {
  config_->Enable(true,
                  false,
                  "quic://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "");
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "PROXY www.bar.com:80;DIRECT", "", "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestFallbackRestricted) {
  config_->Enable(false,
                  true,
                  "https://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "");
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;DIRECT", "", "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestFallbackRestrictedQuic) {
  config_->Enable(false,
                  true,
                  "quic://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "");
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "QUIC www.foo.com:443;DIRECT", "", "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestDisable) {
  config_->Enable(false,
                  false,
                  "https://www.foo.com:443/",
                  "http://www.bar.com:80/",
                  "");
  config_->Disable();
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_NO_RULES, "", "", "");
}

TEST_F(DataReductionProxyConfiguratorTest, TestBypassList) {
  config_->AddHostPatternToBypass("http://www.google.com");
  config_->AddHostPatternToBypass("fefe:13::abc/33");
  config_->AddURLPatternToBypass("foo.org/images/*");
  config_->AddURLPatternToBypass("http://foo.com/*");
  config_->AddURLPatternToBypass("http://baz.com:22/bar/*");
  config_->AddURLPatternToBypass("http://*bat.com/bar/*");

  std::string expected[] = {
    "http://www.google.com",
    "fefe:13::abc/33",
    "foo.org",
    "http://foo.com",
    "http://baz.com:22",
    "http://*bat.com"
  };

  ASSERT_EQ(config_->bypass_rules_.size(), 6u);
  int i = 0;
  for (std::vector<std::string>::iterator it = config_->bypass_rules_.begin();
       it != config_->bypass_rules_.end(); ++it) {
    EXPECT_EQ(expected[i++], *it);
  }
}

}  // namespace data_reduction_proxy

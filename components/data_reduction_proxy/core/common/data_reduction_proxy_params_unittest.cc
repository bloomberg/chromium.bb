// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/proxy/proxy_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kConfigServiceFieldTrial[] = "DataReductionProxyConfigService";
}

namespace data_reduction_proxy {
class DataReductionProxyParamsTest : public testing::Test {
 public:
  void CheckParams(const TestDataReductionProxyParams& params,
                   bool expected_init_result,
                   bool expected_allowed,
                   bool expected_fallback_allowed,
                   bool expected_alternative_allowed,
                   bool expected_promo_allowed) {
    EXPECT_EQ(expected_init_result, params.init_result());
    EXPECT_EQ(expected_allowed, params.allowed());
    EXPECT_EQ(expected_fallback_allowed, params.fallback_allowed());
    EXPECT_EQ(expected_alternative_allowed, params.alternative_allowed());
    EXPECT_EQ(expected_promo_allowed, params.promo_allowed());
  }
  void CheckValues(const TestDataReductionProxyParams& params,
                   const std::string& expected_origin,
                   const std::string& expected_fallback_origin,
                   const std::string& expected_ssl_origin,
                   const std::string& expected_alt_origin,
                   const std::string& expected_alt_fallback_origin,
                   const std::string& expected_secure_proxy_check_url) {
    std::vector<net::ProxyServer> proxies_for_http;
    std::vector<net::ProxyServer> proxies_for_https;
    std::vector<net::ProxyServer> alt_proxies_for_http;
    std::vector<net::ProxyServer> alt_proxies_for_https;
    if (!expected_origin.empty()) {
      proxies_for_http.push_back(net::ProxyServer::FromURI(
          expected_origin, net::ProxyServer::SCHEME_HTTP));
    }

    if (!expected_fallback_origin.empty()) {
      proxies_for_http.push_back(net::ProxyServer::FromURI(
          expected_fallback_origin, net::ProxyServer::SCHEME_HTTP));
    }

    if (!expected_alt_origin.empty()) {
      alt_proxies_for_http.push_back(net::ProxyServer::FromURI(
          expected_alt_origin, net::ProxyServer::SCHEME_HTTP));
    }

    if (!expected_alt_fallback_origin.empty()) {
      alt_proxies_for_http.push_back(net::ProxyServer::FromURI(
          expected_alt_fallback_origin, net::ProxyServer::SCHEME_HTTP));
    }

    if (!expected_ssl_origin.empty()) {
      alt_proxies_for_https.push_back(net::ProxyServer::FromURI(
          expected_ssl_origin, net::ProxyServer::SCHEME_HTTP));
    }

    EXPECT_THAT(proxies_for_http,
                testing::ContainerEq(params.proxies_for_http(false)));
    EXPECT_THAT(proxies_for_https,
                testing::ContainerEq(params.proxies_for_https(false)));
    EXPECT_THAT(alt_proxies_for_http,
                testing::ContainerEq(params.proxies_for_http(true)));
    EXPECT_THAT(alt_proxies_for_https,
                testing::ContainerEq(params.proxies_for_https(true)));
    EXPECT_EQ(GURL(expected_secure_proxy_check_url),
              params.secure_proxy_check_url());
  }
};

TEST_F(DataReductionProxyParamsTest, EverythingDefined) {
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING);
  CheckParams(params, true, true, true, false, true);
  CheckValues(params, TestDataReductionProxyParams::DefaultDevOrigin(),
              TestDataReductionProxyParams::DefaultDevFallbackOrigin(),
              std::string(), std::string(), std::string(),
              TestDataReductionProxyParams::DefaultSecureProxyCheckURL());
}

TEST_F(DataReductionProxyParamsTest, NoDevOrigin) {
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING &
      ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
      ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN);
  CheckParams(params, true, true, true, false, true);
  CheckValues(params, TestDataReductionProxyParams::DefaultOrigin(),
              TestDataReductionProxyParams::DefaultFallbackOrigin(),
              std::string(), std::string(), std::string(),
              TestDataReductionProxyParams::DefaultSecureProxyCheckURL());
}

TEST_F(DataReductionProxyParamsTest, Flags) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxy,
      TestDataReductionProxyParams::FlagOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyFallback,
      TestDataReductionProxyParams::FlagFallbackOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionSSLProxy,
      TestDataReductionProxyParams::FlagSSLOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAlt,
      TestDataReductionProxyParams::FlagAltOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAltFallback,
      TestDataReductionProxyParams::FlagAltFallbackOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxySecureProxyCheckURL,
      TestDataReductionProxyParams::FlagSecureProxyCheckURL());
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kAlternativeAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING);
  CheckParams(params, true, true, true, true, true);
  CheckValues(params, TestDataReductionProxyParams::FlagOrigin(),
              TestDataReductionProxyParams::FlagFallbackOrigin(),
              TestDataReductionProxyParams::FlagSSLOrigin(),
              TestDataReductionProxyParams::FlagAltOrigin(), std::string(),
              TestDataReductionProxyParams::FlagSecureProxyCheckURL());
}

TEST_F(DataReductionProxyParamsTest, CarrierTestFlag) {
  static const char kCarrierTestOrigin[] =
      "http://o-o.preferred.nttdocomodcp-hnd1.proxy-dev.googlezip.net:80";
  base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableDataReductionProxyCarrierTest, kCarrierTestOrigin);
  DataReductionProxyParams params(DataReductionProxyParams::kAllowed);
  std::vector<net::ProxyServer> proxies_for_http;
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      kCarrierTestOrigin, net::ProxyServer::SCHEME_HTTP));
  EXPECT_THAT(params.proxies_for_http(false),
              testing::ContainerEq(proxies_for_http));
}

TEST_F(DataReductionProxyParamsTest, InvalidConfigurations) {
  const struct {
    bool allowed;
    bool fallback_allowed;
    bool alternative_allowed;
    bool alternative_fallback_allowed;
    bool promo_allowed;
    unsigned int missing_definitions;
    bool expected_result;
  } tests[]  = {
    {
      true,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_NOTHING,
      true
    },
    {
      true,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_DEV_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN,
      true
    },
    {
      true,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      false
    },
    {
      true,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      false
    },
    {
      true,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_SECURE_PROXY_CHECK_URL,
      false
    },
    {
      true,
      false,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_NOTHING,
      true
    },
    {
      true,
      false,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      false,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      false,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      false
    },
    {
      true,
      false,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      false
    },
    {
      true,
      false,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      false,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      false,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_SECURE_PROXY_CHECK_URL,
      false
    },
    {
      true,
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_NOTHING,
      true
    },
    {
      true,
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      true
    },
    {
      true,
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      true
    },
    {
      true,
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      true,
      false,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_SECURE_PROXY_CHECK_URL,
      false
    },
    {
      true,
      false,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      false,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      false,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      true
    },
    {
      true,
      false,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      true
    },
    {
      true,
      false,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      false,
      false,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      false,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_SECURE_PROXY_CHECK_URL,
      false
    },
    {
      false,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_NOTHING,
      false
    },
    {
      false,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_SECURE_PROXY_CHECK_URL,
      false
    },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    int flags = 0;
    if (tests[i].allowed)
      flags |= DataReductionProxyParams::kAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    if (tests[i].alternative_allowed)
      flags |= DataReductionProxyParams::kAlternativeAllowed;
    if (tests[i].alternative_fallback_allowed)
      flags |= DataReductionProxyParams::kAlternativeFallbackAllowed;
    if (tests[i].promo_allowed)
      flags |= DataReductionProxyParams::kPromoAllowed;
    TestDataReductionProxyParams params(
        flags,
        TestDataReductionProxyParams::HAS_EVERYTHING &
            ~(tests[i].missing_definitions));
    EXPECT_EQ(tests[i].expected_result, params.init_result()) << i;
  }
}

TEST_F(DataReductionProxyParamsTest, AndroidOnePromoFieldTrial) {
  EXPECT_TRUE(params::IsIncludedInAndroidOnePromoFieldTrial(
      "google/sprout/sprout:4.4.4/KPW53/1379542:user/release-keys"));
  EXPECT_FALSE(params::IsIncludedInAndroidOnePromoFieldTrial(
      "google/hammerhead/hammerhead:5.0/LRX210/1570415:user/release-keys"));
}

TEST_F(DataReductionProxyParamsTest, IsClientConfigEnabled) {
  const struct {
    std::string test_case;
    bool command_line_set;
    std::string trial_group_value;
    bool expected;
  } tests[] = {
      {
       "Nothing set", false, "", false,
      },
      {
       "Command line set", true, "", true,
      },
      {
       "Enabled in experiment", false, "Enabled", true,
      },
      {
       "Alternate enabled in experiment", false, "EnabledOther", true,
      },
      {
       "Disabled in experiment", false, "Disabled", false,
      },
      {
       "Command line set, enabled in experiment", true, "Enabled", true,
      },
      {
       "Command line set, disabled in experiment", true, "Disabled", true,
      },
  };

  for (const auto& test : tests) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);
    if (test.command_line_set) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kEnableDataReductionProxyConfigClient, "");
    }
    base::FieldTrialList field_trial_list(nullptr);
    if (!test.trial_group_value.empty()) {
      base::FieldTrialList::CreateFieldTrial(kConfigServiceFieldTrial,
                                             test.trial_group_value);
    }
    EXPECT_EQ(test.expected, params::IsConfigClientEnabled()) << test.test_case;
  }
}

TEST_F(DataReductionProxyParamsTest, SecureProxyCheckDefault) {
  struct {
    bool command_line_set;
    bool experiment_enabled;
    bool in_trial_group;
    bool expected_use_by_default;
  } test_cases[]{
      {
       false, false, false, true,
      },
      {
       true, false, false, false,
      },
      {
       true, true, false, false,
      },
      {
       true, true, true, false,
      },
      {
       false, true, true, false,
      },
      {
       false, true, false, true,
      },
  };

  int test_index = 0;
  for (const auto& test_case : test_cases) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);

    base::FieldTrialList trial_list(nullptr);
    if (test_case.command_line_set) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyStartSecureDisabled, "");
    }

    if (test_case.experiment_enabled) {
      base::FieldTrialList::CreateFieldTrial(
          "DataReductionProxySecureProxyAfterCheck",
          test_case.in_trial_group ? "Enabled" : "Disabled");
    }

    EXPECT_EQ(test_case.expected_use_by_default,
              params::ShouldUseSecureProxyByDefault())
        << test_index;
    test_index++;
  }
}

TEST_F(DataReductionProxyParamsTest, PopulateConfigResponse) {
  DataReductionProxyParams params(DataReductionProxyParams::kAllowed |
                                  DataReductionProxyParams::kFallbackAllowed);
  ClientConfig config;
  params.PopulateConfigResponse(&config);
  EXPECT_TRUE(config.has_proxy_config());
  EXPECT_EQ(2, config.proxy_config().http_proxy_servers_size());
  const std::vector<net::ProxyServer>& proxies_for_http =
      params.proxies_for_http(false);
  EXPECT_EQ(proxies_for_http[0].host_port_pair().host(),
            config.proxy_config().http_proxy_servers(0).host());
  EXPECT_EQ(proxies_for_http[0].host_port_pair().host(),
            config.proxy_config().http_proxy_servers(0).host());
  EXPECT_EQ(proxies_for_http[1].host_port_pair().host(),
            config.proxy_config().http_proxy_servers(1).host());
  EXPECT_EQ(proxies_for_http[1].host_port_pair().host(),
            config.proxy_config().http_proxy_servers(1).host());
}

}  // namespace data_reduction_proxy

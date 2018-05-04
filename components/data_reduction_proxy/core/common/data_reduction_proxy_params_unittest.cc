// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/proxy_server.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/sys_info.h"
#endif

namespace data_reduction_proxy {
class DataReductionProxyParamsTest : public testing::Test {
 public:
  void CheckValues(const TestDataReductionProxyParams& params,
                   const std::string& expected_origin,
                   const std::string& expected_fallback_origin) {
    std::vector<net::ProxyServer> expected_proxies;
    if (!expected_origin.empty()) {
      expected_proxies.push_back(net::ProxyServer::FromURI(
          expected_origin, net::ProxyServer::SCHEME_HTTP));
    }

    if (!expected_fallback_origin.empty()) {
      expected_proxies.push_back(net::ProxyServer::FromURI(
          expected_fallback_origin, net::ProxyServer::SCHEME_HTTP));
    }

    EXPECT_EQ(expected_proxies,
              DataReductionProxyServer::ConvertToNetProxyServers(
                  params.proxies_for_http()));
  }
};

TEST_F(DataReductionProxyParamsTest, EverythingDefined) {
  TestDataReductionProxyParams params;
  std::vector<DataReductionProxyServer> expected_proxies;

  // Both the origin and fallback proxy must have type CORE.
  expected_proxies.push_back(DataReductionProxyServer(
      net::ProxyServer::FromURI("https://proxy.googlezip.net:443",
                                net::ProxyServer::SCHEME_HTTP),
      ProxyServer::CORE));
  expected_proxies.push_back(DataReductionProxyServer(
      net::ProxyServer::FromURI("compress.googlezip.net:80",
                                net::ProxyServer::SCHEME_HTTP),
      ProxyServer::CORE));

  EXPECT_EQ(expected_proxies, params.proxies_for_http());
}

TEST_F(DataReductionProxyParamsTest, Flags) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxy, "http://ovveride-1.com/");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyFallback, "http://ovveride-2.com/");
  TestDataReductionProxyParams params;
  CheckValues(params, "http://ovveride-1.com/", "http://ovveride-2.com/");
}

TEST_F(DataReductionProxyParamsTest, IsClientConfigEnabled) {
  const struct {
    std::string test_case;
    std::string trial_group_value;
    bool expected;
  } tests[] = {
      {
          "Nothing set", "", true,
      },
      {
          "Enabled in experiment", "Enabled", true,
      },
      {
          "Alternate enabled in experiment", "Enabled_Other", true,
      },
      {
          "Control in experiment", "Control", true,
      },
      {
          "Disabled in experiment", "Disabled", false,
      },
      {
          "Disabled in experiment", "Disabled_Other", false,
      },
      {
          "disabled in experiment lower case", "disabled", true,
      },
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);
    if (!test.trial_group_value.empty()) {
      ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
          "DataReductionProxyConfigService", test.trial_group_value));
    }
    EXPECT_EQ(test.expected, params::IsConfigClientEnabled()) << test.test_case;
  }
}

TEST_F(DataReductionProxyParamsTest, IsBrotliAcceptEncodingEnabled) {
  const struct {
    std::string test_case;
    std::string trial_group_value;
    bool expected;
  } tests[] = {
      {
          "Nothing set", "", true,
      },
      {
          "Enabled in experiment", "Enabled", true,
      },
      {
          "Alternate enabled in experiment", "Enabled_Other", true,
      },
      {
          "Control in experiment", "Control", true,
      },
      {
          "Disabled in experiment", "Disabled", false,
      },
      {
          "Disabled in experiment", "Disabled_Other", false,
      },
      {
          "disabled in experiment lower case", "disabled", true,
      },
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);
    if (!test.trial_group_value.empty()) {
      ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
          "DataReductionProxyBrotliAcceptEncoding", test.trial_group_value));
    }
    EXPECT_EQ(test.expected, params::IsBrotliAcceptEncodingEnabled())
        << test.test_case;
  }
}

TEST_F(DataReductionProxyParamsTest, AreServerExperimentsEnabled) {
  const struct {
    std::string test_case;
    std::string trial_group_value;
    bool disable_flag_set;
    bool expected;
  } tests[] = {
      {
          "Field trial not set", "", false, true,
      },
      {
          "Field trial not set, flag set", "", true, false,
      },
      {
          "Enabled", "Enabled", false, true,
      },
      {
          "Enabled via field trial but disabled via flag", "Enabled", true,
          false,
      },
      {
          "Disabled via field trial", "Disabled", false, false,
      },
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);
    if (!test.trial_group_value.empty()) {
      ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
          "DataReductionProxyServerExperiments", test.trial_group_value));
    }

    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
    if (test.disable_flag_set) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyServerExperimentsDisabled, "");
    }
    EXPECT_EQ(test.expected, params::IsIncludedInServerExperimentsFieldTrial())
        << test.test_case;
  }
}

// Tests if the QUIC field trial is set correctly.
TEST_F(DataReductionProxyParamsTest, QuicFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
    bool enable_warmup_url;
    bool expect_warmup_url_enabled;
    std::string warmup_url;
  } tests[] = {
      {"Enabled", true, true, true, std::string()},
      {"Enabled", true, false, false, std::string()},
      {"Enabled_Control", true, true, true, std::string()},
      {"Control", false, true, false, std::string()},
      {"Disabled", false, true, false, std::string()},
      {"enabled", true, true, true, std::string()},
      {"Enabled", true, true, true, "example.com/test.html"},
      {std::string(), true, false, true, std::string()},
      {"Enabled", true, false, false, std::string()},
  };

  for (const auto& test : tests) {
    ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableDataReductionProxyWarmupURLFetch));

    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> variation_params;
    if (!test.enable_warmup_url)
      variation_params["enable_warmup"] = "false";

    if (!test.warmup_url.empty()) {
      variation_params["warmup_url"] = test.warmup_url;
      variation_params["whitelisted_probe_http_response_code"] = "204";
    }
    ASSERT_TRUE(variations::AssociateVariationParams(
        params::GetQuicFieldTrialName(), test.trial_group_name,
        variation_params));

    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial(params::GetQuicFieldTrialName(),
                                           test.trial_group_name);

    EXPECT_EQ(test.expected_enabled, params::IsIncludedInQuicFieldTrial());
    if (!test.warmup_url.empty()) {
      EXPECT_EQ(GURL(test.warmup_url), params::GetWarmupURL());
      EXPECT_TRUE(params::IsWhitelistedHttpResponseCodeForProbes(200));
      EXPECT_TRUE(params::IsWhitelistedHttpResponseCodeForProbes(net::HTTP_OK));
      EXPECT_TRUE(params::IsWhitelistedHttpResponseCodeForProbes(204));
      EXPECT_FALSE(params::IsWhitelistedHttpResponseCodeForProbes(302));
      EXPECT_FALSE(params::IsWhitelistedHttpResponseCodeForProbes(307));
      EXPECT_TRUE(params::IsWhitelistedHttpResponseCodeForProbes(404));
    } else {
      EXPECT_EQ(GURL("http://check.googlezip.net/e2e_probe"),
                params::GetWarmupURL());
    }
    EXPECT_TRUE(params::FetchWarmupProbeURLEnabled());
  }
}

TEST_F(DataReductionProxyParamsTest, QuicFieldTrialDefaultResponseCodeWarmup) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetch));

  EXPECT_TRUE(params::IsIncludedInQuicFieldTrial());
  EXPECT_EQ(GURL("http://check.googlezip.net/e2e_probe"),
            params::GetWarmupURL());
  EXPECT_TRUE(params::FetchWarmupProbeURLEnabled());

  const struct {
    int http_response_code;
    bool expected_whitelisted;
  } tests[] = {{200, true},
               {net::HTTP_OK, true},
               {204, false},
               {301, false},
               {net::HTTP_TEMPORARY_REDIRECT, false},
               {404, true},
               {net::HTTP_NOT_FOUND, true}};

  for (const auto& test : tests) {
    EXPECT_EQ(test.expected_whitelisted,
              params::IsWhitelistedHttpResponseCodeForProbes(
                  test.http_response_code));
  }
}

// Tests if the QUIC field trial |enable_quic_non_core_proxies| is set
// correctly.
TEST_F(DataReductionProxyParamsTest, QuicEnableNonCoreProxies) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
    std::string enable_non_core_proxies;
    bool expected_enable_non_core_proxies;
  } tests[] = {
      {"Enabled", true, "true", true},        {"Enabled", true, "false", false},
      {"Enabled", true, std::string(), true}, {"Control", false, "true", false},
      {"Disabled", false, "true", false},
  };

  for (const auto& test : tests) {
    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> variation_params;
    variation_params["enable_quic_non_core_proxies"] =
        test.enable_non_core_proxies;

    ASSERT_TRUE(variations::AssociateVariationParams(
        params::GetQuicFieldTrialName(), test.trial_group_name,
        variation_params));

    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial(params::GetQuicFieldTrialName(),
                                           test.trial_group_name);

    EXPECT_EQ(test.expected_enabled, params::IsIncludedInQuicFieldTrial());
    if (params::IsIncludedInQuicFieldTrial()) {
      EXPECT_EQ(test.expected_enable_non_core_proxies,
                params::IsQuicEnabledForNonCoreProxies());
    }
  }
}

TEST_F(DataReductionProxyParamsTest, HoldbackEnabledFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
  } tests[] = {
      {"Enabled", true},
      {"Enabled_Control", true},
      {"Disabled", false},
      {"enabled", false},
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataCompressionProxyHoldback", test.trial_group_name));
    EXPECT_EQ(test.trial_group_name, params::HoldbackFieldTrialGroup());
    EXPECT_EQ(test.expected_enabled, params::IsIncludedInHoldbackFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, PromoFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
  } tests[] = {
      {"Enabled", true},
      {"Enabled_Control", true},
      {"Disabled", false},
      {"enabled", false},
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataCompressionProxyPromoVisibility", test.trial_group_name));
    EXPECT_EQ(test.expected_enabled, params::IsIncludedInPromoFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, FREPromoFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
  } tests[] = {
      {"Enabled", true},
      {"Enabled_Control", true},
      {"Disabled", false},
      {"enabled", false},
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataReductionProxyFREPromo", test.trial_group_name));
    EXPECT_EQ(test.expected_enabled, params::IsIncludedInFREPromoFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, LowMemoryPromoFeature) {
  const struct {
    bool expected_in_field_trial;
  } tests[] = {
      {false}, {true},
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (test.expected_in_field_trial) {
      scoped_feature_list.InitAndDisableFeature(
          features::kDataReductionProxyLowMemoryDevicePromo);
    } else {
      scoped_feature_list.InitAndEnableFeature(
          features::kDataReductionProxyLowMemoryDevicePromo);
    }

#if defined(OS_ANDROID)
    EXPECT_EQ(test.expected_in_field_trial && base::SysInfo::IsLowEndDevice(),
              params::IsIncludedInPromoFieldTrial());
    EXPECT_EQ(test.expected_in_field_trial && base::SysInfo::IsLowEndDevice(),
              params::IsIncludedInFREPromoFieldTrial());
#else
    EXPECT_FALSE(params::IsIncludedInPromoFieldTrial());
    EXPECT_FALSE(params::IsIncludedInFREPromoFieldTrial());
#endif
  }
}

TEST_F(DataReductionProxyParamsTest, GetConfigServiceURL) {
  const struct {
    std::string test_case;
    std::string flag_value;
    GURL expected;
  } tests[] = {
      {
          "Nothing set", "",
          GURL("https://datasaver.googleapis.com/v1/clientConfigs"),
      },
      {
          "Only command line set", "http://commandline.config-service/",
          GURL("http://commandline.config-service/"),
      },
  };

  for (const auto& test : tests) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
    if (!test.flag_value.empty()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyConfigURL, test.flag_value);
    }
    EXPECT_EQ(test.expected, params::GetConfigServiceURL()) << test.test_case;
  }
}

TEST_F(DataReductionProxyParamsTest, SecureProxyURL) {
  const struct {
    std::string test_case;
    std::string flag_value;
    GURL expected;
  } tests[] = {
      {
          "Nothing set", "", GURL("http://check.googlezip.net/connect"),
      },
      {
          "Only command line set", "http://example.com/flag",
          GURL("http://example.com/flag"),
      },
  };

  for (const auto& test : tests) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
    if (!test.flag_value.empty()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxySecureProxyCheckURL, test.flag_value);
    }
    EXPECT_EQ(test.expected, params::GetSecureProxyCheckURL())
        << test.test_case;
  }
}

TEST(DataReductionProxyParamsStandaloneTest, OverrideProxiesForHttp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyHttpProxies,
      "http://override-first.net;http://override-second.net");
  DataReductionProxyParams params;

  // Overriding proxies must have type UNSPECIFIED_TYPE.
  std::vector<DataReductionProxyServer> expected_override_proxies_for_http;
  expected_override_proxies_for_http.push_back(DataReductionProxyServer(
      net::ProxyServer::FromURI("http://override-first.net",
                                net::ProxyServer::SCHEME_HTTP),
      ProxyServer::UNSPECIFIED_TYPE));
  expected_override_proxies_for_http.push_back(DataReductionProxyServer(
      net::ProxyServer::FromURI("http://override-second.net",
                                net::ProxyServer::SCHEME_HTTP),
      ProxyServer::UNSPECIFIED_TYPE));

  EXPECT_EQ(expected_override_proxies_for_http, params.proxies_for_http());
}

TEST(DataReductionProxyParamsStandaloneTest, TestMissingViaHeaderParams) {
  EXPECT_TRUE(
      params::ShouldBypassMissingViaHeader(true /* connection_is_cellular */));
  EXPECT_TRUE(
      params::ShouldBypassMissingViaHeader(false /* connection_is_cellular */));
  std::pair<base::TimeDelta, base::TimeDelta> cell_range =
      params::GetMissingViaHeaderBypassDurationRange(
          true /* connection_is_cellular */);
  EXPECT_EQ(base::TimeDelta::FromMinutes(1), cell_range.first);
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), cell_range.second);
  std::pair<base::TimeDelta, base::TimeDelta> wifi_range =
      params::GetMissingViaHeaderBypassDurationRange(
          false /* connection_is_cellular */);
  EXPECT_EQ(base::TimeDelta::FromMinutes(1), wifi_range.first);
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), wifi_range.second);

  std::map<std::string, std::string> feature_parameters = {
      {"should_bypass_missing_via_cellular", "false"},
      {"missing_via_min_bypass_cellular_in_seconds", "10"},
      {"missing_via_max_bypass_cellular_in_seconds", "20"},
      {"should_bypass_missing_via_wifi", "false"},
      {"missing_via_min_bypass_wifi_in_seconds", "30"},
      {"missing_via_max_bypass_wifi_in_seconds", "40"}};

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kMissingViaHeaderShortDuration, feature_parameters);

  EXPECT_FALSE(
      params::ShouldBypassMissingViaHeader(true /* connection_is_cellular */));
  EXPECT_FALSE(
      params::ShouldBypassMissingViaHeader(false /* connection_is_cellular */));
  cell_range = params::GetMissingViaHeaderBypassDurationRange(
      true /* connection_is_cellular */);
  EXPECT_EQ(base::TimeDelta::FromSeconds(10), cell_range.first);
  EXPECT_EQ(base::TimeDelta::FromSeconds(20), cell_range.second);
  wifi_range = params::GetMissingViaHeaderBypassDurationRange(
      false /* connection_is_cellular */);
  EXPECT_EQ(base::TimeDelta::FromSeconds(30), wifi_range.first);
  EXPECT_EQ(base::TimeDelta::FromSeconds(40), wifi_range.second);
}

}  // namespace data_reduction_proxy

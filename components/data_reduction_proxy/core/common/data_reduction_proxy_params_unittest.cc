// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <map>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    EXPECT_EQ(expected_origin, params.origin().ToURI());
    EXPECT_EQ(expected_fallback_origin, params.fallback_origin().ToURI());
    EXPECT_EQ(expected_ssl_origin, params.ssl_origin().ToURI());
    EXPECT_EQ(expected_alt_origin, params.alt_origin().ToURI());
    EXPECT_EQ(expected_alt_fallback_origin,
              params.alt_fallback_origin().ToURI());
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
  CheckValues(params,
              TestDataReductionProxyParams::DefaultDevOrigin(),
              TestDataReductionProxyParams::DefaultDevFallbackOrigin(),
              TestDataReductionProxyParams::DefaultSSLOrigin(),
              TestDataReductionProxyParams::DefaultAltOrigin(),
              TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
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
  CheckValues(params,
              TestDataReductionProxyParams::DefaultOrigin(),
              TestDataReductionProxyParams::DefaultFallbackOrigin(),
              TestDataReductionProxyParams::DefaultSSLOrigin(),
              TestDataReductionProxyParams::DefaultAltOrigin(),
              TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
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
  CheckValues(params,
              TestDataReductionProxyParams::FlagOrigin(),
              TestDataReductionProxyParams::FlagFallbackOrigin(),
              TestDataReductionProxyParams::FlagSSLOrigin(),
              TestDataReductionProxyParams::FlagAltOrigin(),
              TestDataReductionProxyParams::FlagAltFallbackOrigin(),
              TestDataReductionProxyParams::FlagSecureProxyCheckURL());
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

TEST_F(DataReductionProxyParamsTest, IsDataReductionProxy) {
  const struct {
    net::HostPortPair host_port_pair;
    bool fallback_allowed;
    bool alt_fallback_allowed;
    bool set_dev_origin;
    bool expected_result;
    net::HostPortPair expected_first;
    net::HostPortPair expected_second;
    bool expected_is_fallback;
    bool expected_is_alternative;
    bool expected_is_ssl;
  } tests[]  = {
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        true,
        true,
        false,
        true,
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultFallbackOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        false,
        false,
        false
      },
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        false,
        false,
        false,
        true,
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        net::HostPortPair::FromURL(GURL()),
        false,
        false,
        false
      },
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultFallbackOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        true,
        true,
        false,
        true,
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultFallbackOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        net::HostPortPair::FromURL(GURL()),
        true,
        false,
        false
      },
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultFallbackOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        false,
        false,
        false,
        false,
        net::HostPortPair::FromURL(GURL()),
        net::HostPortPair::FromURL(GURL()),
        false,
        false,
        false
      },
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultAltOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        true,
        true,
        false,
        true,
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultAltOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        false,
        true,
        false
      },
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultAltOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        false,
        false,
        false,
        true,
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultAltOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        net::HostPortPair::FromURL(GURL()),
        false,
        true,
        false
      },
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        true,
        true,
        false,
        true,
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        net::HostPortPair::FromURL(GURL()),
        true,
        true,
        false
      },
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        false,
        false,
        false,
        false,
        net::HostPortPair::FromURL(GURL()),
        net::HostPortPair::FromURL(GURL()),
        false,
        false,
        false
      },
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultSSLOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        true,
        true,
        false,
        true,
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultSSLOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        net::HostPortPair::FromURL(GURL()),
        false,
        false,
        true
      },
      {
        net::ProxyServer::FromURI(
            TestDataReductionProxyParams::DefaultDevOrigin(),
            net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        true,
        true,
        true,
        true,
          net::ProxyServer::FromURI(
              TestDataReductionProxyParams::DefaultDevOrigin(),
              net::ProxyServer::SCHEME_HTTP).host_port_pair(),
          net::ProxyServer::FromURI(
              TestDataReductionProxyParams::DefaultDevFallbackOrigin(),
              net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        false,
        false,
        false
      },
      {
          net::ProxyServer::FromURI(
              TestDataReductionProxyParams::DefaultOrigin(),
              net::ProxyServer::SCHEME_HTTP).host_port_pair(),
        true,
        true,
        true,
        false,
        net::HostPortPair::FromURL(GURL()),
        net::HostPortPair::FromURL(GURL()),
        false,
        false,
        false
      },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    int flags = DataReductionProxyParams::kAllowed |
                DataReductionProxyParams::kAlternativeAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    if (tests[i].alt_fallback_allowed)
      flags |= DataReductionProxyParams::kAlternativeFallbackAllowed;
    unsigned int has_definitions = TestDataReductionProxyParams::HAS_EVERYTHING;
    if (!tests[i].set_dev_origin) {
      has_definitions &= ~TestDataReductionProxyParams::HAS_DEV_ORIGIN;
      has_definitions &= ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN;
    }
    TestDataReductionProxyParams params(flags, has_definitions);
    DataReductionProxyTypeInfo proxy_type_info;
    EXPECT_EQ(tests[i].expected_result,
              params.IsDataReductionProxy(
                  tests[i].host_port_pair, &proxy_type_info)) << i;
    EXPECT_EQ(net::ProxyServer::FromURI(
        tests[i].expected_first.ToString(),
        net::ProxyServer::SCHEME_HTTP).is_valid(),
              proxy_type_info.proxy_servers.first.is_valid()) << i;
    if (proxy_type_info.proxy_servers.first.is_valid()) {
      EXPECT_TRUE(tests[i].expected_first.Equals(
          proxy_type_info.proxy_servers.first.host_port_pair())) << i;
    }
    EXPECT_EQ(net::ProxyServer::FromURI(
        tests[i].expected_second.ToString(),
        net::ProxyServer::SCHEME_HTTP).is_valid(),
              proxy_type_info.proxy_servers.second.is_valid()) << i;
    if (proxy_type_info.proxy_servers.second.is_valid()) {
      EXPECT_TRUE(tests[i].expected_second.Equals(
          proxy_type_info.proxy_servers.second.host_port_pair())) << i;
    }

    EXPECT_EQ(tests[i].expected_is_fallback, proxy_type_info.is_fallback) << i;
    EXPECT_EQ(tests[i].expected_is_alternative, proxy_type_info.is_alternative)
        << i;
    EXPECT_EQ(tests[i].expected_is_ssl, proxy_type_info.is_ssl) << i;
  }
}

std::string GetRetryMapKeyFromOrigin(std::string origin) {
  // The retry map has the scheme prefix for https but not for http
  return origin;
}

TEST_F(DataReductionProxyParamsTest, AreProxiesBypassed) {
  const struct {
    // proxy flags
    bool allowed;
    bool fallback_allowed;
    bool alt_allowed;
    // is https request
    bool is_https;
    // proxies in retry map
    bool origin;
    bool fallback_origin;
    bool alt_origin;
    bool alt_fallback_origin;
    bool ssl_origin;

    bool expected_result;
  } tests[]  = {
      { // proxy flags
        false,
        false,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        false,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        true,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        false,
        false,
        true,
        // is https request
        true,
        // proxies in retry map
        false,
        false,
        false,
        false,
        true,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        true,
        // proxies in retry map
        false,
        false,
        false,
        false,
        true,
        // expected result
        true,
      },
      { // proxy flags
        true,
        false,
        false,
        // is https request
        false,
        // proxies in retry map
        true,
        false,
        false,
        false,
        false,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        true,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        false,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        true,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        true,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        true,
        true,
        false,
        false,
        false,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        true,
        true,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        true,
        false,
        true,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        true,
        true,
        false,
        // expected result
        false,
      },
      { // proxy flags
        false,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        true,
        true,
        false,
        // expected result
        true,
      },
      { // proxy flags
        false,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        true,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        true,
        false,
        true,
        true,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        true,
        true,
        true,
        true,
        true,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        true,
        // proxies in retry map
        true,
        true,
        true,
        true,
        true,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        true,
        // proxies in retry map
        true,
        true,
        true,
        true,
        false,
        // expected result
        false,
      },
  };

  // The retry map has the scheme prefix for https but not for http.
  std::string origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultOrigin());
  std::string fallback_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultFallbackOrigin());
  std::string alt_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultAltOrigin());
  std::string alt_fallback_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultAltFallbackOrigin());
  std::string ssl_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultSSLOrigin());

  for (size_t i = 0; i < arraysize(tests); ++i) {
    net::ProxyConfig::ProxyRules rules;
    std::vector<std::string> proxies;

    if (tests[i].allowed)
      proxies.push_back(origin);
    if (tests[i].alt_allowed)
      proxies.push_back(alt_origin);
    if (tests[i].allowed && tests[i].fallback_allowed)
      proxies.push_back(fallback_origin);
    if (tests[i].alt_allowed && tests[i].fallback_allowed)
      proxies.push_back(alt_fallback_origin);

    std::string proxy_rules = "http=" + JoinString(proxies, ",") + ",direct://;"
        + (tests[i].alt_allowed ? ("https=" + ssl_origin + ",direct://;") : "");

    rules.ParseFromString(proxy_rules);

    int flags = 0;
    if (tests[i].allowed)
      flags |= DataReductionProxyParams::kAllowed;
    if (tests[i].alt_allowed)
      flags |= DataReductionProxyParams::kAlternativeAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    unsigned int has_definitions =
        TestDataReductionProxyParams::HAS_EVERYTHING &
        ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
        ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN;
    TestDataReductionProxyParams params(flags, has_definitions);

    net::ProxyRetryInfoMap retry_map;
    net::ProxyRetryInfo retry_info;
    retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();

    if (tests[i].origin)
      retry_map[origin] = retry_info;
    if (tests[i].fallback_origin)
      retry_map[fallback_origin] = retry_info;
    if (tests[i].alt_origin)
      retry_map[alt_origin] = retry_info;
    if (tests[i].alt_fallback_origin)
      retry_map[alt_fallback_origin] = retry_info;
    if (tests[i].ssl_origin)
      retry_map[ssl_origin] = retry_info;

    bool was_bypassed = params.AreProxiesBypassed(retry_map,
                                                  rules,
                                                  tests[i].is_https,
                                                  NULL);

    EXPECT_EQ(tests[i].expected_result, was_bypassed);
  }
}

TEST_F(DataReductionProxyParamsTest, AreProxiesBypassedRetryDelay) {
  std::string origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultOrigin());
  std::string fallback_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultFallbackOrigin());

  net::ProxyConfig::ProxyRules rules;
  std::vector<std::string> proxies;

  proxies.push_back(origin);
  proxies.push_back(fallback_origin);

  std::string proxy_rules = "http=" + JoinString(proxies, ",") + ",direct://;";

  rules.ParseFromString(proxy_rules);

  int flags = 0;
  flags |= DataReductionProxyParams::kAllowed;
  flags |= DataReductionProxyParams::kFallbackAllowed;
  unsigned int has_definitions =
      TestDataReductionProxyParams::HAS_EVERYTHING &
      ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
      ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN;
  TestDataReductionProxyParams params(flags, has_definitions);

  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo retry_info;

  retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();
  retry_map[origin] = retry_info;

  retry_info.bad_until = base::TimeTicks();
  retry_map[fallback_origin] = retry_info;

  bool was_bypassed = params.AreProxiesBypassed(retry_map,
                                                rules,
                                                false,
                                                NULL);

  EXPECT_FALSE(was_bypassed);

  base::TimeDelta delay = base::TimeDelta::FromHours(2);
  retry_info.bad_until = base::TimeTicks::Now() + delay;
  retry_info.current_delay = delay;
  retry_map[origin] = retry_info;

  delay = base::TimeDelta::FromHours(1);
  retry_info.bad_until = base::TimeTicks::Now() + delay;
  retry_info.current_delay = delay;
  retry_map[fallback_origin] = retry_info;

  base::TimeDelta min_retry_delay;
  was_bypassed = params.AreProxiesBypassed(retry_map,
                                           rules,
                                           false,
                                           &min_retry_delay);
  EXPECT_TRUE(was_bypassed);
  EXPECT_EQ(delay, min_retry_delay);
}

TEST_F(DataReductionProxyParamsTest, AndroidOnePromoFieldTrial) {
  EXPECT_TRUE(DataReductionProxyParams::IsIncludedInAndroidOnePromoFieldTrial(
      "google/sprout/sprout:4.4.4/KPW53/1379542:user/release-keys"));
  EXPECT_FALSE(DataReductionProxyParams::IsIncludedInAndroidOnePromoFieldTrial(
      "google/hammerhead/hammerhead:5.0/LRX210/1570415:user/release-keys"));
}

}  // namespace data_reduction_proxy

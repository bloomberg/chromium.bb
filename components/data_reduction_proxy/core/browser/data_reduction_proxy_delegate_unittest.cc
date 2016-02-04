// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"

#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/proxy/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace data_reduction_proxy {

namespace {

// Constructs and returns a proxy with the specified scheme.
net::ProxyServer GetProxyWithScheme(net::ProxyServer::Scheme scheme) {
  switch (scheme) {
    case net::ProxyServer::SCHEME_HTTP:
      return net::ProxyServer::FromURI("origin.net:443",
                                       net::ProxyServer::SCHEME_HTTP);
    case net::ProxyServer::SCHEME_HTTPS:
      return net::ProxyServer::FromURI("https://origin.net:443",
                                       net::ProxyServer::SCHEME_HTTP);
    case net::ProxyServer::SCHEME_QUIC:
      return net::ProxyServer::FromURI("quic://origin.net:443",
                                       net::ProxyServer::SCHEME_QUIC);
    default:
      NOTREACHED();
      return net::ProxyServer::FromURI("", net::ProxyServer::SCHEME_INVALID);
  }
}

// Tests that the trusted SPDY proxy is verified correctly.
TEST(DataReductionProxyDelegate, IsTrustedSpdyProxy) {
  base::MessageLoopForIO message_loop_;
  scoped_ptr<DataReductionProxyTestContext> test_context =
      DataReductionProxyTestContext::Builder()
          .WithConfigClient()
          .WithTestConfigurator()
          .WithMockDataReductionProxyService()
          .Build();

  const struct {
    bool is_in_trusted_spdy_proxy_field_trial;
    net::ProxyServer::Scheme first_proxy_scheme;
    net::ProxyServer::Scheme second_proxy_scheme;
    bool expect_proxy_is_trusted;
  } tests[] = {
      {false, net::ProxyServer::SCHEME_HTTP, net::ProxyServer::SCHEME_INVALID,
       false},
      {true, net::ProxyServer::SCHEME_HTTP, net::ProxyServer::SCHEME_INVALID,
       false},
      {true, net::ProxyServer::SCHEME_QUIC, net::ProxyServer::SCHEME_INVALID,
       false},
      {true, net::ProxyServer::SCHEME_HTTP, net::ProxyServer::SCHEME_HTTP,
       false},
      {true, net::ProxyServer::SCHEME_INVALID, net::ProxyServer::SCHEME_INVALID,
       false},
      // First proxy is HTTPS, and second is invalid.
      {true, net::ProxyServer::SCHEME_HTTPS, net::ProxyServer::SCHEME_INVALID,
       true},
      // First proxy is invalid, and second proxy is HTTPS.
      {true, net::ProxyServer::SCHEME_INVALID, net::ProxyServer::SCHEME_HTTPS,
       true},
      // First proxy is HTTPS, and second is HTTP.
      {true, net::ProxyServer::SCHEME_HTTPS, net::ProxyServer::SCHEME_HTTPS,
       true},
      // Second proxy is HTTPS, and first is HTTP.
      {true, net::ProxyServer::SCHEME_HTTP, net::ProxyServer::SCHEME_HTTPS,
       true},
      {true, net::ProxyServer::SCHEME_QUIC, net::ProxyServer::SCHEME_INVALID,
       false},
      {true, net::ProxyServer::SCHEME_QUIC, net::ProxyServer::SCHEME_HTTP,
       false},
      {true, net::ProxyServer::SCHEME_QUIC, net::ProxyServer::SCHEME_HTTPS,
       true},
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    ASSERT_EQ(
        tests[i].expect_proxy_is_trusted,
        tests[i].is_in_trusted_spdy_proxy_field_trial &&
            (tests[i].first_proxy_scheme == net::ProxyServer::SCHEME_HTTPS ||
             tests[i].second_proxy_scheme == net::ProxyServer::SCHEME_HTTPS))
        << i;

    std::vector<net::ProxyServer> proxies_for_http;
    net::ProxyServer first_proxy;
    net::ProxyServer second_proxy;
    if (tests[i].first_proxy_scheme != net::ProxyServer::SCHEME_INVALID) {
      first_proxy = GetProxyWithScheme(tests[i].first_proxy_scheme);
      proxies_for_http.push_back(first_proxy);
    }
    if (tests[i].second_proxy_scheme != net::ProxyServer::SCHEME_INVALID) {
      second_proxy = GetProxyWithScheme(tests[i].second_proxy_scheme);
      proxies_for_http.push_back(second_proxy);
    }

    scoped_ptr<DataReductionProxyMutableConfigValues> config_values =
        DataReductionProxyMutableConfigValues::CreateFromParams(
            test_context->test_params());
    config_values->UpdateValues(proxies_for_http);

    scoped_ptr<DataReductionProxyConfig> config(new DataReductionProxyConfig(
        test_context->net_log(), std::move(config_values),
        test_context->configurator(), test_context->event_creator()));

    DataReductionProxyDelegate delegate(
        test_context->io_data()->request_options(), config.get());

    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial(
        params::GetTrustedSpdyProxyFieldTrialName(),
        tests[i].is_in_trusted_spdy_proxy_field_trial ? "Enabled" : "Control");

    EXPECT_EQ(tests[i].expect_proxy_is_trusted,
              delegate.IsTrustedSpdyProxy(first_proxy) ||
                  delegate.IsTrustedSpdyProxy(second_proxy))
        << i;
  }
}

}  // namespace

}  // namespace data_reduction_proxy
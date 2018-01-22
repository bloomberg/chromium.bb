// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class DataReductionProxyMutableConfigValuesTest : public testing::Test {
 public:
  DataReductionProxyMutableConfigValuesTest() {}
  ~DataReductionProxyMutableConfigValuesTest() override {}

  void Init() {
    mutable_config_values_ =
        std::make_unique<DataReductionProxyMutableConfigValues>();
  }

  DataReductionProxyMutableConfigValues* mutable_config_values() const {
    return mutable_config_values_.get();
  }

 private:
  std::unique_ptr<DataReductionProxyMutableConfigValues> mutable_config_values_;
};

TEST_F(DataReductionProxyMutableConfigValuesTest, UpdateValuesAndInvalidate) {
  Init();
  EXPECT_EQ(std::vector<DataReductionProxyServer>(),
            mutable_config_values()->proxies_for_http());

  std::vector<DataReductionProxyServer> proxies_for_http;

  net::ProxyServer first_proxy_server(net::ProxyServer::FromURI(
      "http://first.net", net::ProxyServer::SCHEME_HTTP));
  proxies_for_http.push_back(
      DataReductionProxyServer(first_proxy_server, ProxyServer::CORE));

  net::ProxyServer second_proxy_server = net::ProxyServer::FromURI(
      "http://second.net", net::ProxyServer::SCHEME_HTTP);
  proxies_for_http.push_back(DataReductionProxyServer(
      second_proxy_server, ProxyServer::UNSPECIFIED_TYPE));

  mutable_config_values()->UpdateValues(proxies_for_http);
  EXPECT_EQ(proxies_for_http, mutable_config_values()->proxies_for_http());

  // Invalidation must clear out the list of proxies and their properties.
  mutable_config_values()->Invalidate();
  EXPECT_TRUE(mutable_config_values()->proxies_for_http().empty());
}

// Tests if HTTP proxies are overridden when |kDataReductionProxyHttpProxies|
// switch is specified.
TEST_F(DataReductionProxyMutableConfigValuesTest, OverrideProxiesForHttp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyHttpProxies,
      "http://override-first.net;http://override-second.net");
  Init();

  EXPECT_EQ(std::vector<DataReductionProxyServer>(),
            mutable_config_values()->proxies_for_http());

  std::vector<DataReductionProxyServer> proxies_for_http;

  net::ProxyServer first_proxy_server(net::ProxyServer::FromURI(
      "http://first.net", net::ProxyServer::SCHEME_HTTP));
  proxies_for_http.push_back(
      DataReductionProxyServer(first_proxy_server, ProxyServer::CORE));

  net::ProxyServer second_proxy_server = net::ProxyServer::FromURI(
      "http://second.net", net::ProxyServer::SCHEME_HTTP);
  proxies_for_http.push_back(DataReductionProxyServer(
      second_proxy_server, ProxyServer::UNSPECIFIED_TYPE));

  mutable_config_values()->UpdateValues(proxies_for_http);

  std::vector<DataReductionProxyServer> expected_override_proxies_for_http;
  expected_override_proxies_for_http.push_back(DataReductionProxyServer(
      net::ProxyServer::FromURI("http://override-first.net",
                                net::ProxyServer::SCHEME_HTTP),
      ProxyServer::UNSPECIFIED_TYPE));
  expected_override_proxies_for_http.push_back(DataReductionProxyServer(
      net::ProxyServer::FromURI("http://override-second.net",
                                net::ProxyServer::SCHEME_HTTP),
      ProxyServer::UNSPECIFIED_TYPE));

  EXPECT_EQ(expected_override_proxies_for_http,
            mutable_config_values()->proxies_for_http());

  // Invalidation must clear out the list of proxies and their properties.
  mutable_config_values()->Invalidate();
  EXPECT_TRUE(mutable_config_values()->proxies_for_http().empty());
}

// Tests if HTTP proxies are overridden when |kDataReductionProxy| or
// |kDataReductionProxyFallback| switches are specified.
TEST_F(DataReductionProxyMutableConfigValuesTest, OverrideDataReductionProxy) {
  const struct {
    bool set_primary;
    bool set_fallback;
  } tests[] = {
      {false, false}, {true, false}, {false, true}, {true, true},
  };

  for (const auto& test : tests) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
    if (test.set_primary) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxy, "http://override-first.net");
    }
    if (test.set_fallback) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyFallback, "http://override-second.net");
    }
    Init();

    EXPECT_EQ(std::vector<DataReductionProxyServer>(),
              mutable_config_values()->proxies_for_http());

    std::vector<DataReductionProxyServer> proxies_for_http;

    if (test.set_primary) {
      net::ProxyServer first_proxy_server = (net::ProxyServer::FromURI(
          "http://first.net", net::ProxyServer::SCHEME_HTTP));
      proxies_for_http.push_back(
          DataReductionProxyServer(first_proxy_server, ProxyServer::CORE));
    }
    if (test.set_fallback) {
      net::ProxyServer second_proxy_server = net::ProxyServer::FromURI(
          "http://second.net", net::ProxyServer::SCHEME_HTTP);

      proxies_for_http.push_back(DataReductionProxyServer(
          second_proxy_server, ProxyServer::UNSPECIFIED_TYPE));
    }

    mutable_config_values()->UpdateValues(proxies_for_http);

    // Overriding proxies must have type UNSPECIFIED_TYPE.
    std::vector<DataReductionProxyServer> expected_override_proxies_for_http;
    if (test.set_primary) {
      expected_override_proxies_for_http.push_back(DataReductionProxyServer(
          net::ProxyServer::FromURI("http://override-first.net",
                                    net::ProxyServer::SCHEME_HTTP),
          ProxyServer::UNSPECIFIED_TYPE));
    }
    if (test.set_fallback) {
      expected_override_proxies_for_http.push_back(DataReductionProxyServer(
          net::ProxyServer::FromURI("http://override-second.net",
                                    net::ProxyServer::SCHEME_HTTP),
          ProxyServer::UNSPECIFIED_TYPE));
    }

    EXPECT_EQ(expected_override_proxies_for_http,
              mutable_config_values()->proxies_for_http());

    // Invalidation must clear out the list of proxies and their properties.
    mutable_config_values()->Invalidate();
    EXPECT_TRUE(mutable_config_values()->proxies_for_http().empty());
  }
}

}  // namespace

}  // namespace data_reduction_proxy

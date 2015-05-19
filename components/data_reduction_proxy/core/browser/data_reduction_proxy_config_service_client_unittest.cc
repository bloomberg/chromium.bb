// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kSuccessResponse[] =
    "{ \"sessionKey\": \"SecretSessionKey\", "
    "\"expireTime\": \"1970-01-01T00:01:00.000Z\", "
    "\"proxyConfig\": { \"httpProxyServers\": ["
    "{ \"scheme\": \"HTTPS\", \"host\": \"origin.net\", \"port\": 443 },"
    "{ \"scheme\": \"HTTP\", \"host\": \"fallback.net\", \"port\": 80 }"
    "] } }";
// The following values should match the ones in the response above.
const char kSuccessOrigin[] = "https://origin.net:443";
const char kSuccessFallback[] = "fallback.net:80";
const char kSuccessSessionKey[] = "SecretSessionKey";

}  // namespace

namespace data_reduction_proxy {

namespace {

class RequestOptionsPopulator {
 public:
  RequestOptionsPopulator(const base::Time& expiration_time,
                          const base::TimeDelta& increment)
      : expiration_time_(expiration_time),
        increment_(increment) {
  }

  void PopulateResponse(base::DictionaryValue* response) {
    response->SetString("sessionKey", "abcdef|1234-5678-12345678");
    response->SetString("expireTime",
                        config_parser::TimeToISO8601(expiration_time_));
    expiration_time_ += increment_;
  }

 private:
  base::Time expiration_time_;
  base::TimeDelta increment_;
};

void PopulateResponseFailure(base::DictionaryValue* response) {
}

}  // namespace

class DataReductionProxyConfigServiceClientTest : public testing::Test {
 protected:
  DataReductionProxyConfigServiceClientTest() : context_(true) {}

  void SetUp() override {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsDefinitions(TestDataReductionProxyParams::HAS_EVERYTHING)
            .WithURLRequestContext(&context_)
            .WithMockClientSocketFactory(&mock_socket_factory_)
            .WithTestConfigurator()
            .WithMockRequestOptions()
            .WithTestConfigClient()
            .Build();
    context_.set_client_socket_factory(&mock_socket_factory_);
    context_.Init();
    ResetBackoffEntryReleaseTime();
    test_context_->test_config_client()->SetNow(base::Time::UnixEpoch());
    enabled_proxies_for_http_ =
        test_context_->test_params()->proxies_for_http(false /* alternative */);
  }

  void SetDataReductionProxyEnabled(bool enabled) {
    test_context_->config()->SetStateForTest(enabled, false, true);
  }

  scoped_ptr<DataReductionProxyConfigServiceClient> BuildConfigClient() {
    scoped_ptr<DataReductionProxyParams> params(new DataReductionProxyParams(
        DataReductionProxyParams::kAllowed |
        DataReductionProxyParams::kFallbackAllowed |
        DataReductionProxyParams::kPromoAllowed));
    request_options_.reset(new DataReductionProxyRequestOptions(
        Client::UNKNOWN, test_context_->io_data()->config()));
    return scoped_ptr<DataReductionProxyConfigServiceClient>(
        new DataReductionProxyConfigServiceClient(
            params.Pass(), GetBackoffPolicy(), request_options_.get(),
            test_context_->mutable_config_values(),
            test_context_->io_data()->config(), test_context_->event_creator(),
            test_context_->net_log()));
  }

  void ResetBackoffEntryReleaseTime() {
    config_client()->SetCustomReleaseTime(base::TimeTicks::UnixEpoch());
  }

  void VerifyRemoteSuccess() {
    std::vector<net::ProxyServer> expected_http_proxies;
    expected_http_proxies.push_back(net::ProxyServer::FromURI(
        kSuccessOrigin, net::ProxyServer::SCHEME_HTTP));
    expected_http_proxies.push_back(net::ProxyServer::FromURI(
        kSuccessFallback, net::ProxyServer::SCHEME_HTTP));
    EXPECT_EQ(base::TimeDelta::FromMinutes(1), config_client()->GetDelay());
    EXPECT_THAT(expected_http_proxies,
                testing::ContainerEq(configurator()->proxies_for_http()));
    EXPECT_TRUE(configurator()->proxies_for_https().empty());
    EXPECT_EQ(kSuccessSessionKey, request_options()->GetSecureSession());
  }

  DataReductionProxyParams* params() {
    return test_context_->test_params();
  }

  TestDataReductionProxyConfigServiceClient* config_client() {
    return test_context_->test_config_client();
  }

  TestDataReductionProxyConfigurator* configurator() {
    return test_context_->test_configurator();
  }

  MockDataReductionProxyRequestOptions* request_options() {
    return test_context_->mock_request_options();
  }

  const std::vector<net::ProxyServer>& enabled_proxies_for_http() const {
    return enabled_proxies_for_http_;
  }

  void RunUntilIdle() {
    test_context_->RunUntilIdle();
  }

  net::MockClientSocketFactory* mock_socket_factory() {
    return &mock_socket_factory_;
  }

 private:
  base::MessageLoopForIO message_loop_;
  net::TestURLRequestContext context_;
  net::MockClientSocketFactory mock_socket_factory_;

  scoped_ptr<DataReductionProxyTestContext> test_context_;
  scoped_ptr<DataReductionProxyRequestOptions> request_options_;
  std::vector<net::ProxyServer> enabled_proxies_for_http_;
};

TEST_F(DataReductionProxyConfigServiceClientTest, TestConstructStaticResponse) {
  scoped_ptr<DataReductionProxyConfigServiceClient> config_client =
      BuildConfigClient();
  std::string config_data = config_client->ConstructStaticResponse();
  ClientConfig config;
  EXPECT_TRUE(config_parser::ParseClientConfig(config_data, &config));
}

TEST_F(DataReductionProxyConfigServiceClientTest, SuccessfulLoop) {
  // Use a local/static config.
  config_client()->SetConfigServiceURL(GURL());
  RequestOptionsPopulator populator(
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(1),
      base::TimeDelta::FromDays(1));
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke(&populator,
                          &RequestOptionsPopulator::PopulateResponse));
  config_client()->RetrieveConfig();
  EXPECT_EQ(base::TimeDelta::FromDays(1), config_client()->GetDelay());
  EXPECT_THAT(enabled_proxies_for_http(),
              testing::ContainerEq(configurator()->proxies_for_http()));
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  config_client()->SetNow(base::Time::UnixEpoch() + base::TimeDelta::FromDays(1)
                          + base::TimeDelta::FromSeconds(5));
  config_client()->RetrieveConfig();
  EXPECT_EQ(base::TimeDelta::FromDays(1) - base::TimeDelta::FromSeconds(5),
            config_client()->GetDelay());
  EXPECT_THAT(enabled_proxies_for_http(),
              testing::ContainerEq(configurator()->proxies_for_http()));
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
}

TEST_F(DataReductionProxyConfigServiceClientTest, SuccessfulLoopShortDuration) {
  // Use a local/static config.
  config_client()->SetConfigServiceURL(GURL());
  RequestOptionsPopulator populator(
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromSeconds(1));
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(&populator,
                                &RequestOptionsPopulator::PopulateResponse));
  config_client()->RetrieveConfig();
  EXPECT_EQ(base::TimeDelta::FromSeconds(10), config_client()->GetDelay());
  EXPECT_THAT(enabled_proxies_for_http(),
              testing::ContainerEq(configurator()->proxies_for_http()));
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
}

TEST_F(DataReductionProxyConfigServiceClientTest, EnsureBackoff) {
  // Use a local/static config.
  config_client()->SetConfigServiceURL(GURL());
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_))
      .Times(2)
      .WillRepeatedly(testing::Invoke(&PopulateResponseFailure));
  config_client()->RetrieveConfig();
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_EQ(base::TimeDelta::FromSeconds(20), config_client()->GetDelay());
  config_client()->RetrieveConfig();
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_EQ(base::TimeDelta::FromSeconds(40), config_client()->GetDelay());
}

TEST_F(DataReductionProxyConfigServiceClientTest, ConfigDisabled) {
  // Use a local/static config.
  config_client()->SetConfigServiceURL(GURL());
  RequestOptionsPopulator populator(
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(1),
      base::TimeDelta::FromDays(1));
  SetDataReductionProxyEnabled(false);
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(&populator,
                                &RequestOptionsPopulator::PopulateResponse));
  config_client()->RetrieveConfig();
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_EQ(base::TimeDelta::FromDays(1), config_client()->GetDelay());
}

TEST_F(DataReductionProxyConfigServiceClientTest, GetConfigServiceURL) {
  const struct {
    std::string flag_value;
    GURL expected;
  } tests[] = {
      {
       "", GURL(),
      },
      {
       "http://configservice.chrome-test.com",
       GURL("http://configservice.chrome-test.com"),
      },
  };

  for (const auto& test : tests) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kDataReductionProxyConfigURL, test.flag_value);
    EXPECT_EQ(test.expected,
              DataReductionProxyConfigServiceClient::GetConfigServiceURL(
                  *base::CommandLine::ForCurrentProcess()));
  }
}

TEST_F(DataReductionProxyConfigServiceClientTest, RemoteConfigSuccess) {
  net::MockRead mock_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead(kSuccessResponse),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider socket_data_provider(
      mock_reads, arraysize(mock_reads), nullptr, 0);
  mock_socket_factory()->AddSocketDataProvider(&socket_data_provider);
  config_client()->SetConfigServiceURL(GURL("http://configservice.com"));
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_)).Times(0);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess();
}

TEST_F(DataReductionProxyConfigServiceClientTest,
       RemoteConfigSuccessAfterFailure) {
  net::MockRead mock_reads_array[][3] = {
      {
       // Failure due to 404 error.
       net::MockRead("HTTP/1.1 404 Not found\r\n\r\n"),
       net::MockRead(""),
       net::MockRead(net::SYNCHRONOUS, net::OK),
      },
      {
       // Success.
       net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
       net::MockRead(kSuccessResponse),
       net::MockRead(net::SYNCHRONOUS, net::OK),
      },
  };

  ScopedVector<net::SocketDataProvider> socket_data_providers;
  for (net::MockRead* mock_reads : mock_reads_array) {
    socket_data_providers.push_back(
        new net::StaticSocketDataProvider(mock_reads, 3, nullptr, 0));
    mock_socket_factory()->AddSocketDataProvider(socket_data_providers.back());
  }

  config_client()->SetConfigServiceURL(GURL("http://configservice.com"));
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_)).Times(0);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  EXPECT_EQ(base::TimeDelta::FromSeconds(20), config_client()->GetDelay());
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_TRUE(request_options()->GetSecureSession().empty());
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess();
}

TEST_F(DataReductionProxyConfigServiceClientTest, OnIPAddressChange) {
  config_client()->SetConfigServiceURL(GURL("http://configservice.com"));
  SetDataReductionProxyEnabled(true);
  config_client()->RetrieveConfig();
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_)).Times(0);

  static const int kFailureCount = 5;

  net::MockRead failure_reads[] = {
      net::MockRead("HTTP/1.1 404 Not found\r\n\r\n"),
      net::MockRead(""),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };

  ScopedVector<net::SocketDataProvider> socket_data_providers;
  for (int i = 0; i < kFailureCount; ++i) {
    socket_data_providers.push_back(
        new net::StaticSocketDataProvider(failure_reads, 3, nullptr, 0));
    mock_socket_factory()->AddSocketDataProvider(socket_data_providers.back());
    config_client()->RetrieveConfig();
    RunUntilIdle();
  }

  EXPECT_EQ(base::TimeDelta::FromSeconds(320), config_client()->GetDelay());
  EXPECT_EQ(kFailureCount, config_client()->GetBackoffErrorCount());
  config_client()->OnIPAddressChanged();
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  ResetBackoffEntryReleaseTime();

  net::MockRead success_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead(kSuccessResponse),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider socket_data_provider(
      success_reads, arraysize(success_reads), nullptr, 0);
  mock_socket_factory()->AddSocketDataProvider(&socket_data_provider);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess();
}

}  // namespace data_reduction_proxy

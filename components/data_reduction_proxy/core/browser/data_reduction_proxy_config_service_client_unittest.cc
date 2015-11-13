// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include <map>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/time/tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/variations/variations_associated_data.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// The following values should match those in
// DataReductionProxyConfigServiceClientTest.config_:
const char kSuccessOrigin[] = "https://origin.net:443";
const char kSuccessFallback[] = "fallback.net:80";
const char kSuccessSessionKey[] = "SecretSessionKey";

// The following values should match those in
// DataReductionProxyConfigServiceClientTest.previous_config_:
const char kOldSuccessOrigin[] = "https://old.origin.net:443";
const char kOldSuccessFallback[] = "old.fallback.net:80";
const char kOldSuccessSessionKey[] = "OldSecretSessionKey";

// The following values should match those in
// DataReductionProxyConfigServiceClientTest.loaded_config_:
const char kPersistedOrigin[] = "https://persisted.net:443";
const char kPersistedFallback[] = "persisted.net:80";
const char kPersistedSessionKey[] = "PersistedSessionKey";

}  // namespace

namespace data_reduction_proxy {

namespace {

class RequestOptionsPopulator {
 public:
  RequestOptionsPopulator(const base::TimeDelta& increment)
      : increment_(increment) {}

  void PopulateResponse(ClientConfig* config) {
    config->set_session_key("abcdef|1234-5678-12345678");
    config_parser::TimeDeltatoDuration(increment_,
                                       config->mutable_refresh_duration());
  }

 private:
  base::TimeDelta increment_;
};

void PopulateResponseFailure(ClientConfig* config) {
}

void StoreSerializedConfig(const std::string& value) {
}

// Creates a new ClientConfig from the given parameters.
ClientConfig CreateConfig(const std::string& session_key,
                          int64 expire_duration_seconds,
                          int64 expire_duration_nanoseconds,
                          ProxyServer_ProxyScheme primary_scheme,
                          const std::string& primary_host,
                          int primary_port,
                          ProxyServer_ProxyScheme secondary_scheme,
                          const std::string& secondary_host,
                          int secondary_port) {
  ClientConfig config;

  config.set_session_key(session_key);
  config.mutable_refresh_duration()->set_seconds(expire_duration_seconds);
  config.mutable_refresh_duration()->set_nanos(expire_duration_nanoseconds);
  ProxyServer* primary_proxy =
      config.mutable_proxy_config()->add_http_proxy_servers();
  primary_proxy->set_scheme(primary_scheme);
  primary_proxy->set_host(primary_host);
  primary_proxy->set_port(primary_port);
  ProxyServer* secondary_proxy =
      config.mutable_proxy_config()->add_http_proxy_servers();
  secondary_proxy->set_scheme(secondary_scheme);
  secondary_proxy->set_host(secondary_host);
  secondary_proxy->set_port(secondary_port);

  return config;
}

// Takes |config| and returns the base64 encoding of its serialized byte stream.
std::string EncodeConfig(const ClientConfig& config) {
  std::string config_data;
  std::string encoded_data;
  EXPECT_TRUE(config.SerializeToString(&config_data));
  base::Base64Encode(config_data, &encoded_data);
  return encoded_data;
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
    test_context_->test_config_client()->SetEnabled(true);
    enabled_proxies_for_http_ =
        test_context_->test_params()->proxies_for_http();

    // Set up the various test ClientConfigs.
    ClientConfig config = CreateConfig(
        kSuccessSessionKey, 600, 0, ProxyServer_ProxyScheme_HTTPS, "origin.net",
        443, ProxyServer_ProxyScheme_HTTP, "fallback.net", 80);
    config.SerializeToString(&config_);
    encoded_config_ = EncodeConfig(config);

    ClientConfig previous_config =
        CreateConfig(kOldSuccessSessionKey, 600, 0,
                     ProxyServer_ProxyScheme_HTTPS, "old.origin.net", 443,
                     ProxyServer_ProxyScheme_HTTP, "old.fallback.net", 80);
    previous_config.SerializeToString(&previous_config_);

    ClientConfig persisted =
        CreateConfig(kPersistedSessionKey, 600, 0,
                     ProxyServer_ProxyScheme_HTTPS, "persisted.net", 443,
                     ProxyServer_ProxyScheme_HTTP, "persisted.net", 80);
    loaded_config_ = EncodeConfig(persisted);
  }

  void SetDataReductionProxyEnabled(bool enabled) {
    test_context_->config()->SetStateForTest(enabled, true);
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
            test_context_->net_log(), base::Bind(&StoreSerializedConfig)));
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
    EXPECT_EQ(base::TimeDelta::FromMinutes(10), config_client()->GetDelay());
    EXPECT_THAT(configurator()->proxies_for_http(),
                testing::ContainerEq(expected_http_proxies));
    EXPECT_TRUE(configurator()->proxies_for_https().empty());
    EXPECT_EQ(kSuccessSessionKey, request_options()->GetSecureSession());
    EXPECT_EQ(encoded_config(), persisted_config());
  }

  void VerifyRemoteSuccessWithOldConfig() {
    std::vector<net::ProxyServer> expected_http_proxies;
    expected_http_proxies.push_back(net::ProxyServer::FromURI(
        kOldSuccessOrigin, net::ProxyServer::SCHEME_HTTP));
    expected_http_proxies.push_back(net::ProxyServer::FromURI(
        kOldSuccessFallback, net::ProxyServer::SCHEME_HTTP));
    EXPECT_EQ(base::TimeDelta::FromMinutes(10), config_client()->GetDelay());
    EXPECT_THAT(configurator()->proxies_for_http(),
                testing::ContainerEq(expected_http_proxies));
    EXPECT_TRUE(configurator()->proxies_for_https().empty());
    EXPECT_EQ(kOldSuccessSessionKey, request_options()->GetSecureSession());
  }

  void VerifySuccessWithLoadedConfig() {
    std::vector<net::ProxyServer> expected_http_proxies;
    expected_http_proxies.push_back(net::ProxyServer::FromURI(
        kPersistedOrigin, net::ProxyServer::SCHEME_HTTP));
    expected_http_proxies.push_back(net::ProxyServer::FromURI(
        kPersistedFallback, net::ProxyServer::SCHEME_HTTP));
    EXPECT_THAT(configurator()->proxies_for_http(),
                testing::ContainerEq(expected_http_proxies));
    EXPECT_TRUE(configurator()->proxies_for_https().empty());
    EXPECT_EQ(kPersistedSessionKey, request_options()->GetSecureSession());
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

  std::string persisted_config() const {
    return test_context_->pref_service()->GetString(
        prefs::kDataReductionProxyConfig);
  }

  const std::string& success_response() const { return config_; }

  const std::string& encoded_config() const { return encoded_config_; }

  const std::string& previous_success_response() const {
    return previous_config_;
  }

  void EnableQuic(bool enable) { params()->EnableQuic(enable); }

  const std::string& loaded_config() const { return loaded_config_; }

 private:
  base::MessageLoopForIO message_loop_;
  net::TestURLRequestContext context_;
  net::MockClientSocketFactory mock_socket_factory_;

  scoped_ptr<DataReductionProxyTestContext> test_context_;
  scoped_ptr<DataReductionProxyRequestOptions> request_options_;
  std::vector<net::ProxyServer> enabled_proxies_for_http_;

  // A configuration from the current remote request. The encoded version is
  // also stored.
  std::string config_;
  std::string encoded_config_;

  // A configuration from a previous remote request.
  std::string previous_config_;

  // An encoded config that represents a previously saved configuration.
  std::string loaded_config_;
};

TEST_F(DataReductionProxyConfigServiceClientTest, TestConstructStaticResponse) {
  scoped_ptr<DataReductionProxyConfigServiceClient> config_client =
      BuildConfigClient();
  std::string config_data = config_client->ConstructStaticResponse();
  ClientConfig config;
  EXPECT_TRUE(config.ParseFromString(config_data));
}

TEST_F(DataReductionProxyConfigServiceClientTest, SuccessfulLoop) {
  // Use a local/static config.
  config_client()->SetConfigServiceURL(GURL());
  RequestOptionsPopulator populator(base::TimeDelta::FromDays(1));
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
  EXPECT_THAT(configurator()->proxies_for_http(),
              testing::ContainerEq(enabled_proxies_for_http()));
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  config_client()->SetNow(base::Time::UnixEpoch() + base::TimeDelta::FromDays(1)
                          + base::TimeDelta::FromSeconds(5));
  config_client()->RetrieveConfig();
  // Now that we use a duration, fetching 5s after expiration should still
  // give a 1d delay.
  EXPECT_EQ(base::TimeDelta::FromDays(1), config_client()->GetDelay());
  EXPECT_THAT(configurator()->proxies_for_http(),
              testing::ContainerEq(enabled_proxies_for_http()));
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
}

// Tests the interaction of client config with dev rollout and QUIC field trial.
TEST_F(DataReductionProxyConfigServiceClientTest, DevRolloutAndQuic) {
  const struct {
    bool enable_dev;
    bool enable_quic;
    std::string expected_primary_proxy;
    std::string expected_fallback_proxy;
    net::ProxyServer::Scheme expected_primary_proxy_scheme;
  } tests[] = {
      {false, false, kSuccessOrigin, kSuccessFallback,
       net::ProxyServer::SCHEME_HTTPS},
      {false, true, kSuccessOrigin, kSuccessFallback,
       net::ProxyServer::SCHEME_QUIC},
      {true, false, TestDataReductionProxyParams::DefaultDevOrigin(),
       TestDataReductionProxyParams::DefaultDevFallbackOrigin(),
       net::ProxyServer::SCHEME_HTTPS},
      {true, true, TestDataReductionProxyParams::DefaultDevOrigin(),
       TestDataReductionProxyParams::DefaultDevFallbackOrigin(),
       net::ProxyServer::SCHEME_QUIC},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    if (tests[i].enable_dev) {
      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      command_line->AppendSwitch(
          data_reduction_proxy::switches::kEnableDataReductionProxyDev);
    }

    base::FieldTrialList field_trial_list(new base::MockEntropyProvider());
    if (tests[i].enable_quic) {
      base::FieldTrialList::CreateFieldTrial(params::GetQuicFieldTrialName(),
                                             "Enabled");
    } else {
      base::FieldTrialList::CreateFieldTrial(params::GetQuicFieldTrialName(),
                                             "Control");
    }
    EnableQuic(tests[i].enable_quic);

    // Use a remote config.
    net::MockRead mock_reads[] = {
        net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
        net::MockRead(net::ASYNC, success_response().c_str(),
                      success_response().length()),
        net::MockRead(net::SYNCHRONOUS, net::OK),
    };
    net::StaticSocketDataProvider socket_data_provider(
        mock_reads, arraysize(mock_reads), nullptr, 0);
    mock_socket_factory()->AddSocketDataProvider(&socket_data_provider);
    config_client()->SetConfigServiceURL(GURL("http://configservice.com"));

    RequestOptionsPopulator populator(base::TimeDelta::FromDays(1));
    SetDataReductionProxyEnabled(true);

    config_client()->RetrieveConfig();
    RunUntilIdle();
    EXPECT_EQ(base::TimeDelta::FromMinutes(10), config_client()->GetDelay())
        << i;

    std::vector<net::ProxyServer> proxies_for_http =
        configurator()->proxies_for_http();

    EXPECT_EQ(2U, proxies_for_http.size()) << i;
    EXPECT_EQ(net::ProxyServer(tests[i].expected_primary_proxy_scheme,
                               net::ProxyServer::FromURI(
                                   tests[i].expected_primary_proxy,
                                   tests[i].expected_primary_proxy_scheme)
                                   .host_port_pair()),
              proxies_for_http[0])
        << i;
    EXPECT_EQ(net::ProxyServer::FromURI(tests[i].expected_fallback_proxy,
                                        net::ProxyServer::SCHEME_HTTP),
              proxies_for_http[1])
        << i;
    EXPECT_TRUE(configurator()->proxies_for_https().empty()) << i;
  }
}

TEST_F(DataReductionProxyConfigServiceClientTest, SuccessfulLoopShortDuration) {
  // Use a local/static config.
  config_client()->SetConfigServiceURL(GURL());
  RequestOptionsPopulator populator(base::TimeDelta::FromSeconds(1));
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(&populator,
                                &RequestOptionsPopulator::PopulateResponse));
  config_client()->RetrieveConfig();
  EXPECT_EQ(config_client()->minimum_refresh_interval_on_success(),
            config_client()->GetDelay());
  EXPECT_THAT(configurator()->proxies_for_http(),
              testing::ContainerEq(enabled_proxies_for_http()));
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
  RequestOptionsPopulator populator(base::TimeDelta::FromDays(1));
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

TEST_F(DataReductionProxyConfigServiceClientTest, RemoteConfigSuccess) {
  net::MockRead mock_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead(net::ASYNC, success_response().c_str(),
                    success_response().length()),
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
       net::MockRead(net::ASYNC, success_response().c_str(),
                     success_response().length()),
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
      net::MockRead(net::ASYNC, success_response().c_str(),
                    success_response().length()),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider socket_data_provider(
      success_reads, arraysize(success_reads), nullptr, 0);
  mock_socket_factory()->AddSocketDataProvider(&socket_data_provider);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess();
}

TEST_F(DataReductionProxyConfigServiceClientTest, OnIPAddressChangeDisabled) {
  config_client()->SetEnabled(false);
  config_client()->SetConfigServiceURL(GURL("http://configservice.com"));
  SetDataReductionProxyEnabled(true);
  config_client()->RetrieveConfig();
  EXPECT_CALL(*request_options(), PopulateConfigResponse(testing::_)).Times(0);

  static const int kFailureCount = 5;

  for (int i = 0; i < kFailureCount; ++i) {
    config_client()->RetrieveConfig();
    RunUntilIdle();
  }

  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  config_client()->OnIPAddressChanged();
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());

  config_client()->RetrieveConfig();
  RunUntilIdle();

  EXPECT_TRUE(request_options()->GetSecureSession().empty());
}

TEST_F(DataReductionProxyConfigServiceClientTest, AuthFailure) {
  net::MockRead mock_reads_array[][3] = {
      {
       // Success.
       net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
       net::MockRead(net::ASYNC, previous_success_response().c_str(),
                     previous_success_response().length()),
       net::MockRead(net::SYNCHRONOUS, net::OK),
      },
      {
       // Success.
       net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
       net::MockRead(net::ASYNC, success_response().c_str(),
                     success_response().length()),
       net::MockRead(net::SYNCHRONOUS, net::OK),
      },
      {
       // Success.
       net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
       net::MockRead(net::ASYNC, previous_success_response().c_str(),
                     previous_success_response().length()),
       net::MockRead(net::SYNCHRONOUS, net::OK),
      },
  };
  ScopedVector<net::SocketDataProvider> socket_data_providers;
  base::HistogramTester histogram_tester;
  for (net::MockRead* mock_reads : mock_reads_array) {
    socket_data_providers.push_back(
        new net::StaticSocketDataProvider(mock_reads, 3, nullptr, 0));
    mock_socket_factory()->AddSocketDataProvider(socket_data_providers.back());
  }

  config_client()->SetConfigServiceURL(GURL("http://configservice.com"));
  SetDataReductionProxyEnabled(true);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.AuthExpired", 0);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccessWithOldConfig();
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);

  scoped_refptr<net::HttpResponseHeaders> parsed(new net::HttpResponseHeaders(
      "HTTP/1.1 407 Proxy Authentication Required\n"));
  net::ProxyServer origin = net::ProxyServer::FromURI(
      kOldSuccessOrigin, net::ProxyServer::SCHEME_HTTP);
  EXPECT_TRUE(config_client()->ShouldRetryDueToAuthFailure(
      parsed.get(), origin.host_port_pair()));
  EXPECT_EQ(1, config_client()->GetBackoffErrorCount());
  EXPECT_EQ(std::string(), persisted_config());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", true, 1);
  RunUntilIdle();
  VerifyRemoteSuccess();

  origin =
      net::ProxyServer::FromURI(kSuccessOrigin, net::ProxyServer::SCHEME_HTTP);
  EXPECT_TRUE(config_client()->ShouldRetryDueToAuthFailure(
      parsed.get(), origin.host_port_pair()));
  EXPECT_EQ(2, config_client()->GetBackoffErrorCount());
  RunUntilIdle();
  VerifyRemoteSuccessWithOldConfig();
}

TEST_F(DataReductionProxyConfigServiceClientTest, ApplySerializedConfig) {
  net::MockRead success_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead(net::ASYNC, success_response().c_str(),
                    success_response().length()),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider socket_data_provider(
      success_reads, arraysize(success_reads), nullptr, 0);
  mock_socket_factory()->AddSocketDataProvider(&socket_data_provider);

  config_client()->SetConfigServiceURL(GURL("http://configservice.com"));
  SetDataReductionProxyEnabled(true);
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
  config_client()->ApplySerializedConfig(loaded_config());
  VerifySuccessWithLoadedConfig();
  EXPECT_EQ(std::string(), persisted_config());

  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess();
}

TEST_F(DataReductionProxyConfigServiceClientTest,
       ApplySerializedConfigAfterReceipt) {
  net::MockRead success_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead(net::ASYNC, success_response().c_str(),
                    success_response().length()),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider socket_data_provider(
      success_reads, arraysize(success_reads), nullptr, 0);
  mock_socket_factory()->AddSocketDataProvider(&socket_data_provider);

  config_client()->SetConfigServiceURL(GURL("http://configservice.com"));
  SetDataReductionProxyEnabled(true);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess();

  config_client()->ApplySerializedConfig(encoded_config());
  VerifyRemoteSuccess();
}

TEST_F(DataReductionProxyConfigServiceClientTest, ApplySerializedConfigLocal) {
  SetDataReductionProxyEnabled(true);
  config_client()->ApplySerializedConfig(encoded_config());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
}

// Tests if the client config field trial parameters are parsed correctly.
TEST_F(DataReductionProxyConfigServiceClientTest,
       ClientConfigFieldTrialParams) {
  const struct {
    bool field_trial_enabled;
    base::TimeDelta expected_min_delay;

  } tests[] = {
      {false, config_client()->minimum_refresh_interval_on_success()},
      {true, base::TimeDelta::FromMilliseconds(120)},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> variation_params;

    if (tests[i].field_trial_enabled) {
      variation_params["minimum_refresh_interval_on_success_msec"] =
          base::Int64ToString(tests[i].expected_min_delay.InMilliseconds());
    }

    ASSERT_TRUE(variations::AssociateVariationParams(
        params::GetClientConfigFieldTrialName(), "Enabled", variation_params));

    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial(
        params::GetClientConfigFieldTrialName(), "Enabled");

    config_client()->PopulateClientConfigParams();

    EXPECT_EQ(tests[i].expected_min_delay,
              config_client()->minimum_refresh_interval_on_success_)
        << i;
  }
}

}  // namespace data_reduction_proxy

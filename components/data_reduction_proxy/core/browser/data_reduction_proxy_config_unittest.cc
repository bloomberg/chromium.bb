// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/previews/core/previews_decider.h"
#include "components/previews/core/previews_experiments.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_status_code.h"
#include "net/log/test_net_log.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/external_estimate_provider.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/proxy/proxy_server.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace data_reduction_proxy {

namespace {

void SetProxiesForHttpOnCommandLine(
    const std::vector<net::ProxyServer>& proxies_for_http) {
  std::vector<std::string> proxy_strings;
  for (const net::ProxyServer& proxy : proxies_for_http)
    proxy_strings.push_back(proxy.ToURI());

  // Proxies specified via kDataReductionProxyHttpProxies command line switch
  // have type ProxyServer::UNSPECIFIED_TYPE.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyHttpProxies,
      base::JoinString(proxy_strings, ";"));
}

std::string GetRetryMapKeyFromOrigin(const std::string& origin) {
  // The retry map has the scheme prefix for https but not for http.
  return origin;
}

class TestPreviewsDecider : public previews::PreviewsDecider {
 public:
  TestPreviewsDecider(bool allow_previews) : allow_previews_(allow_previews) {}
  ~TestPreviewsDecider() override {}

  // previews::PreviewsDecider:
  bool ShouldAllowPreviewAtECT(
      const net::URLRequest& request,
      previews::PreviewsType type,
      net::EffectiveConnectionType effective_connection_type_threshold,
      const std::vector<std::string>& host_blacklist_from_server)
      const override {
    return allow_previews_;
  }
  bool ShouldAllowPreview(const net::URLRequest& request,
                          previews::PreviewsType type) const override {
    return allow_previews_;
  }

 private:
  bool allow_previews_;
};

}  // namespace

class DataReductionProxyConfigTest : public testing::Test {
 public:
  DataReductionProxyConfigTest() : mock_config_used_(false) {}
  ~DataReductionProxyConfigTest() override {}

  void SetUp() override {
    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
    base::RunLoop().RunUntilIdle();
    network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());

    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithMockDataReductionProxyService()
                        .Build();

    ResetSettings();

    expected_params_.reset(new TestDataReductionProxyParams());
  }

  void RecreateContextWithMockConfig() {
    mock_config_used_ = true;
    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithMockConfig()
                        .WithMockDataReductionProxyService()
                        .Build();

    ResetSettings();

    expected_params_.reset(new TestDataReductionProxyParams());
  }

  void ResetSettings() {
    if (mock_config_used_)
      mock_config()->ResetParamFlagsForTest();
    else
      test_config()->ResetParamFlagsForTest();
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() {
    return message_loop_.task_runner();
  }

  class TestResponder {
   public:
    void ExecuteCallback(SecureProxyCheckerCallback callback) {
      callback.Run(response, status, http_response_code);
    }

    std::string response;
    net::URLRequestStatus status;
    int http_response_code;
  };

  void CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::ConnectionType connection_type,
      const std::string& response,
      bool is_captive_portal,
      int response_code,
      net::URLRequestStatus status,
      SecureProxyCheckFetchResult expected_fetch_result,
      const std::vector<net::ProxyServer>& expected_proxies_for_http) {
    base::HistogramTester histogram_tester;
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        connection_type);

    TestResponder responder;
    responder.response = response;
    responder.status = status;
    responder.http_response_code = response_code;
    EXPECT_CALL(*mock_config(), SecureProxyCheck(_))
        .Times(1)
        .WillRepeatedly(testing::WithArgs<0>(
            testing::Invoke(&responder, &TestResponder::ExecuteCallback)));
    mock_config()->SetIsCaptivePortal(is_captive_portal);
    test_context_->RunUntilIdle();
    EXPECT_EQ(expected_proxies_for_http, GetConfiguredProxiesForHttp());

    if (!status.is_success() &&
        status.error() != net::ERR_INTERNET_DISCONNECTED) {
      histogram_tester.ExpectUniqueSample("DataReductionProxy.ProbeURLNetError",
                                          std::abs(status.error()), 1);
    } else {
      histogram_tester.ExpectTotalCount("DataReductionProxy.ProbeURLNetError",
                                        0);
    }
    histogram_tester.ExpectUniqueSample("DataReductionProxy.ProbeURL",
                                        expected_fetch_result, 1);

    // Recorded on every IP change.
    histogram_tester.ExpectUniqueSample(
        "DataReductionProxy.CaptivePortalDetected.Platform", is_captive_portal,
        1);
  }

  void RunUntilIdle() {
    test_context_->RunUntilIdle();
  }

  std::unique_ptr<DataReductionProxyConfig> BuildConfig(
      std::unique_ptr<DataReductionProxyParams> params) {
    return base::MakeUnique<DataReductionProxyConfig>(
        task_runner(), test_context_->net_log(), std::move(params),
        test_context_->configurator(), test_context_->event_creator());
  }

  MockDataReductionProxyConfig* mock_config() {
    DCHECK(mock_config_used_);
    return test_context_->mock_config();
  }

  TestDataReductionProxyConfig* test_config() {
    DCHECK(!mock_config_used_);
    return test_context_->config();
  }

  DataReductionProxyConfigurator* configurator() const {
    return test_context_->configurator();
  }

  TestDataReductionProxyParams* params() const {
    return expected_params_.get();
  }

  DataReductionProxyEventCreator* event_creator() const {
    return test_context_->event_creator();
  }

  net::NetLog* net_log() const { return test_context_->net_log(); }

  std::vector<net::ProxyServer> GetConfiguredProxiesForHttp() const {
    return test_context_->GetConfiguredProxiesForHttp();
  }

  base::MessageLoopForIO message_loop_;

  std::unique_ptr<DataReductionProxyTestContext> test_context_;

 private:
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<TestDataReductionProxyParams> expected_params_;

  bool mock_config_used_;
};

TEST_F(DataReductionProxyConfigTest, TestReloadConfigHoldback) {
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));

  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);
  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});

  ResetSettings();

  test_config()->UpdateConfigForTesting(true, false, true);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
}

TEST_F(DataReductionProxyConfigTest, TestOnConnectionChangePersistedData) {
  base::FieldTrialList field_trial_list(nullptr);

  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);
  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});

  ResetSettings();
  test_config()->SetProxyConfig(true, true);

  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  test_config()->SetCurrentNetworkID("wifi,test");
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  test_config()->UpdateConfigForTesting(true /* enabled */,
                                        false /* secure_proxies_allowed */,
                                        true /* insecure_proxies_allowed */);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  base::RunLoop().RunUntilIdle();

  test_config()->SetCurrentNetworkID("cell,test");
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_2G);
  base::RunLoop().RunUntilIdle();
  test_config()->UpdateConfigForTesting(true /* enabled */,
                                        false /* secure_proxies_allowed */,
                                        false /* insecure_proxies_allowed */);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({}), GetConfiguredProxiesForHttp());

  // On network change, persisted config should be read, and config reloaded.
  test_config()->SetCurrentNetworkID("wifi,test");
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
}

TEST_F(DataReductionProxyConfigTest, TestOnNetworkChanged) {
  RecreateContextWithMockConfig();
  const net::URLRequestStatus kSuccess(net::URLRequestStatus::SUCCESS, net::OK);
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings();

  // The proxy is enabled initially.
  mock_config()->UpdateConfigForTesting(true, true, true);
  mock_config()->OnNewClientConfigFetched();

  // Connection change triggers a secure proxy check that succeeds. Proxy
  // remains unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, "OK", false, net::HTTP_OK,
      kSuccess, SUCCEEDED_PROXY_ALREADY_ENABLED, {kHttpsProxy, kHttpProxy});

  // Connection change triggers a secure proxy check that succeeds but captive
  // portal fails. Proxy is restricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, "OK", true, net::HTTP_OK,
      kSuccess, SUCCEEDED_PROXY_ALREADY_ENABLED,
      std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that fails. Proxy is
  // restricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, "Bad", false, net::HTTP_OK,
      kSuccess, FAILED_PROXY_DISABLED,
      std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that succeeds. Proxies
  // are unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, "OK", false, net::HTTP_OK,
      kSuccess, SUCCEEDED_PROXY_ENABLED, {kHttpsProxy, kHttpProxy});

  // Connection change triggers a secure proxy check that fails. Proxy is
  // restricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, "Bad", true, net::HTTP_OK,
      kSuccess, FAILED_PROXY_DISABLED,
      std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that fails due to the
  // network changing again. This should be ignored, so the proxy should remain
  // unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, std::string(), false,
      net::URLFetcher::RESPONSE_CODE_INVALID,
      net::URLRequestStatus(net::URLRequestStatus::FAILED,
                            net::ERR_INTERNET_DISCONNECTED),
      INTERNET_DISCONNECTED, std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that fails. Proxy remains
  // restricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, "Bad", false, net::HTTP_OK,
      kSuccess, FAILED_PROXY_ALREADY_DISABLED,
      std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that succeeds. Proxy is
  // unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, "OK", false, net::HTTP_OK,
      kSuccess, SUCCEEDED_PROXY_ENABLED, {kHttpsProxy, kHttpProxy});

  // Connection change triggers a secure proxy check that fails due to the
  // network changing again. This should be ignored, so the proxy should remain
  // unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, std::string(), false,
      net::URLFetcher::RESPONSE_CODE_INVALID,
      net::URLRequestStatus(net::URLRequestStatus::FAILED,
                            net::ERR_INTERNET_DISCONNECTED),
      INTERNET_DISCONNECTED, {kHttpsProxy, kHttpProxy});

  // Connection change triggers a secure proxy check that fails because of a
  // redirect response, e.g. by a captive portal. Proxy is restricted.
  CheckSecureProxyCheckOnNetworkChange(
      net::NetworkChangeNotifier::CONNECTION_WIFI, "Bad", false,
      net::HTTP_FOUND,
      net::URLRequestStatus(net::URLRequestStatus::CANCELED, net::ERR_ABORTED),
      FAILED_PROXY_DISABLED, std::vector<net::ProxyServer>(1, kHttpProxy));
}

// Verifies that the warm up URL is fetched correctly.
TEST_F(DataReductionProxyConfigTest, WarmupURL) {
  const net::URLRequestStatus kSuccess(net::URLRequestStatus::SUCCESS, net::OK);
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);

  // Set up the embedded test server from where the warm up URL will be fetched.
  net::EmbeddedTestServer embedded_test_server;
  embedded_test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  EXPECT_TRUE(embedded_test_server.Start());

  GURL warmup_url = embedded_test_server.GetURL("/simple.html");

  const struct {
    bool data_reduction_proxy_enabled;
  } tests[] = {
      {
          false,
      },
      {
          true,
      },
  };
  for (const auto& test : tests) {
    SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
    ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableDataReductionProxyWarmupURLFetch));

    ResetSettings();

    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> variation_params;
    variation_params["warmup_url"] = warmup_url.spec();

    ASSERT_TRUE(variations::AssociateVariationParams(
        params::GetQuicFieldTrialName(), "Enabled", variation_params));

    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial(params::GetQuicFieldTrialName(),
                                           "Enabled");

    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
    TestDataReductionProxyConfig config(task_runner(), nullptr, configurator(),
                                        event_creator());

    scoped_refptr<net::URLRequestContextGetter> request_context_getter_ =
        new net::TestURLRequestContextGetter(task_runner());

    NetworkPropertiesManager network_properties_manager(
        test_context_->pref_service(), test_context_->task_runner());
    config.InitializeOnIOThread(request_context_getter_.get(),
                                request_context_getter_.get(),
                                &network_properties_manager);
    RunUntilIdle();

    {
      base::HistogramTester histogram_tester;

      // Set the connection type to WiFi so that warm up URL is fetched even if
      // the test device does not have connectivity.
      config.connection_type_ = net::NetworkChangeNotifier::CONNECTION_WIFI;
      config.SetProxyConfig(test.data_reduction_proxy_enabled, true);
      ASSERT_TRUE(params::FetchWarmupURLEnabled());

      if (test.data_reduction_proxy_enabled) {
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            0 /* kFetchInitiated */, 1);
      } else {
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            2 /* kProxyNotEnabledByUser */, 1);
      }
    }

    {
      base::HistogramTester histogram_tester;
      // Set the connection type to 4G so that warm up URL is fetched even if
      // the test device does not have connectivity.
      net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
          net::NetworkChangeNotifier::CONNECTION_4G);
      RunUntilIdle();

      if (test.data_reduction_proxy_enabled) {
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent", 2);
        histogram_tester.ExpectBucketCount(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            2 /* kProxyNotEnabledByUser */, 1);
        histogram_tester.ExpectBucketCount(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            0 /* kFetchInitiated */, 1);
      } else {
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchInitiated", 0);
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchSuccessful", 0);
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            2 /* kProxyNotEnabledByUser */, 2);
      }
    }

    {
      base::HistogramTester histogram_tester;
      // Warm up URL should not be fetched since the device does not have
      // connectivity.
      net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
          net::NetworkChangeNotifier::CONNECTION_NONE);
      RunUntilIdle();

      if (test.data_reduction_proxy_enabled) {
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchInitiated", 0);
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent", 2);
        histogram_tester.ExpectBucketCount(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            1 /* kConnectionTypeNone */, 1);
        histogram_tester.ExpectBucketCount(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            2 /* kProxyNotEnabledByUser */, 1);

      } else {
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchInitiated", 0);
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchSuccessful", 0);
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            2 /* kProxyNotEnabledByUser */, 2);
      }
    }
  }
}

TEST_F(DataReductionProxyConfigTest, AreProxiesBypassed) {
  const struct {
    // proxy flags
    bool allowed;
    bool fallback_allowed;
    // is https request
    bool is_https;
    // proxies in retry map
    bool origin;
    bool fallback_origin;

    bool expected_result;
  } tests[] = {
      {
          // proxy flags
          false, false,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          false, false,
          // is https request
          true,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          false, true,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, false,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, false,
          // is https request
          false,
          // proxies in retry map
          true, false,
          // expected result
          true,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          true, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          true, true,
          // expected result
          true,
      },
      {
          // proxy flags
          true, true,
          // is https request
          true,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          true,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          false, true,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          true,
          // proxies in retry map
          true, true,
          // expected result
          false,
      },
  };

  // The retry map has the scheme prefix for https but not for http.
  std::string origin = GetRetryMapKeyFromOrigin(params()
                                                    ->proxies_for_http()
                                                    .front()
                                                    .proxy_server()
                                                    .host_port_pair()
                                                    .ToString());
  std::string fallback_origin =
      GetRetryMapKeyFromOrigin(params()
                                   ->proxies_for_http()
                                   .at(1)
                                   .proxy_server()
                                   .host_port_pair()
                                   .ToString());

  for (size_t i = 0; i < arraysize(tests); ++i) {
    net::ProxyConfig::ProxyRules rules;
    std::vector<std::string> proxies;

    if (tests[i].allowed)
      proxies.push_back(origin);
    if (tests[i].allowed && tests[i].fallback_allowed)
      proxies.push_back(fallback_origin);

    std::string proxy_rules = "http=" + base::JoinString(proxies, ",") +
                              ",direct://;";

    rules.ParseFromString(proxy_rules);

    std::unique_ptr<TestDataReductionProxyParams> params(
        new TestDataReductionProxyParams());
    std::unique_ptr<DataReductionProxyConfig> config =
        BuildConfig(std::move(params));

    net::ProxyRetryInfoMap retry_map;
    net::ProxyRetryInfo retry_info;
    retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();

    if (tests[i].origin)
      retry_map[origin] = retry_info;
    if (tests[i].fallback_origin)
      retry_map[fallback_origin] = retry_info;

    bool was_bypassed = config->AreProxiesBypassed(retry_map, rules,
                                                   tests[i].is_https, nullptr);

    EXPECT_EQ(tests[i].expected_result, was_bypassed) << i;
  }
}

TEST_F(DataReductionProxyConfigTest, AreProxiesBypassedRetryDelay) {
  std::string origin = GetRetryMapKeyFromOrigin(params()
                                                    ->proxies_for_http()
                                                    .front()
                                                    .proxy_server()
                                                    .host_port_pair()
                                                    .ToString());
  std::string fallback_origin =
      GetRetryMapKeyFromOrigin(params()
                                   ->proxies_for_http()
                                   .at(1)
                                   .proxy_server()
                                   .host_port_pair()
                                   .ToString());

  net::ProxyConfig::ProxyRules rules;
  std::vector<std::string> proxies;

  proxies.push_back(origin);
  proxies.push_back(fallback_origin);

  std::string proxy_rules =
      "http=" + base::JoinString(proxies, ",") + ",direct://;";

  rules.ParseFromString(proxy_rules);

  std::unique_ptr<TestDataReductionProxyParams> params(
      new TestDataReductionProxyParams());
  std::unique_ptr<DataReductionProxyConfig> config =
      BuildConfig(std::move(params));

  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo retry_info;

  retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();
  retry_map[origin] = retry_info;

  retry_info.bad_until = base::TimeTicks();
  retry_map[fallback_origin] = retry_info;

  bool was_bypassed =
      config->AreProxiesBypassed(retry_map, rules, false, nullptr);

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
  was_bypassed = config->AreProxiesBypassed(retry_map,
                                            rules,
                                            false,
                                            &min_retry_delay);
  EXPECT_TRUE(was_bypassed);
  EXPECT_EQ(delay, min_retry_delay);
}

TEST_F(DataReductionProxyConfigTest, IsDataReductionProxyWithParams) {
  const struct {
    net::ProxyServer proxy_server;
    bool expected_result;
    net::ProxyServer expected_first;
    net::ProxyServer expected_second;
    bool expected_is_fallback;
  } tests[] = {
      {params()->proxies_for_http().front().proxy_server(), true,
       params()->proxies_for_http().front().proxy_server(),
       params()->proxies_for_http().at(1).proxy_server(), false},
      {params()->proxies_for_http().at(1).proxy_server(), true,
       params()->proxies_for_http().at(1).proxy_server(), net::ProxyServer(),
       true},
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<TestDataReductionProxyParams> params(
        new TestDataReductionProxyParams());
    DataReductionProxyTypeInfo proxy_type_info;
    std::unique_ptr<DataReductionProxyConfig> config(
        new DataReductionProxyConfig(task_runner(), net_log(),
                                     std::move(params), configurator(),
                                     event_creator()));
    EXPECT_EQ(
        tests[i].expected_result,
        config->IsDataReductionProxy(tests[i].proxy_server, &proxy_type_info))
        << i;
    EXPECT_EQ(tests[i].expected_first.is_valid(),
              proxy_type_info.proxy_servers.size() >= 1 &&
                  proxy_type_info.proxy_servers[0].is_valid())
        << i;
    if (proxy_type_info.proxy_servers.size() >= 1 &&
        proxy_type_info.proxy_servers[0].is_valid()) {
      EXPECT_EQ(tests[i].expected_first, proxy_type_info.proxy_servers[0]) << i;
    }
    EXPECT_EQ(tests[i].expected_second.is_valid(),
              proxy_type_info.proxy_servers.size() >= 2 &&
                  proxy_type_info.proxy_servers[1].is_valid())
        << i;
    if (proxy_type_info.proxy_servers.size() >= 2 &&
        proxy_type_info.proxy_servers[1].is_valid()) {
      EXPECT_EQ(tests[i].expected_second, proxy_type_info.proxy_servers[1])
          << i;
    }

    EXPECT_EQ(tests[i].expected_is_fallback, proxy_type_info.proxy_index != 0)
        << i;
  }
}

TEST_F(DataReductionProxyConfigTest, IsDataReductionProxyWithMutableConfig) {
  std::vector<DataReductionProxyServer> proxies_for_http;
  proxies_for_http.push_back(DataReductionProxyServer(
      net::ProxyServer::FromURI("https://origin.net:443",
                                net::ProxyServer::SCHEME_HTTP),
      ProxyServer::CORE));
  proxies_for_http.push_back(DataReductionProxyServer(
      net::ProxyServer::FromURI("http://origin.net:80",
                                net::ProxyServer::SCHEME_HTTP),
      ProxyServer::CORE));

  proxies_for_http.push_back(DataReductionProxyServer(
      net::ProxyServer::FromURI("quic://anotherorigin.net:443",
                                net::ProxyServer::SCHEME_HTTP),
      ProxyServer::CORE));

  const struct {
    DataReductionProxyServer proxy_server;
    bool expected_result;
    std::vector<DataReductionProxyServer> expected_proxies;
    size_t expected_proxy_index;
  } tests[] = {
      {
          proxies_for_http[0], true,
          std::vector<DataReductionProxyServer>(proxies_for_http.begin(),
                                                proxies_for_http.end()),
          0,
      },
      {
          proxies_for_http[1], true,
          std::vector<DataReductionProxyServer>(proxies_for_http.begin() + 1,
                                                proxies_for_http.end()),
          1,
      },
      {
          proxies_for_http[2], true,
          std::vector<DataReductionProxyServer>(proxies_for_http.begin() + 2,
                                                proxies_for_http.end()),
          2,
      },
      {
          DataReductionProxyServer(net::ProxyServer(),
                                   ProxyServer::UNSPECIFIED_TYPE),
          false, std::vector<DataReductionProxyServer>(), 0,
      },
      {
          DataReductionProxyServer(
              net::ProxyServer(
                  net::ProxyServer::SCHEME_HTTPS,
                  net::HostPortPair::FromString("otherorigin.net:443")),
              ProxyServer::UNSPECIFIED_TYPE),
          false, std::vector<DataReductionProxyServer>(), 0,
      },
      {
          // Verifies that when determining if a proxy is a valid data reduction
          // proxy, only the host port pairs are compared.
          DataReductionProxyServer(
              net::ProxyServer::FromURI("origin.net:443",
                                        net::ProxyServer::SCHEME_QUIC),
              ProxyServer::UNSPECIFIED_TYPE),
          true, std::vector<DataReductionProxyServer>(proxies_for_http.begin(),
                                                      proxies_for_http.end()),
          0,
      },
      {
          DataReductionProxyServer(
              net::ProxyServer::FromURI("origin2.net:443",
                                        net::ProxyServer::SCHEME_HTTPS),
              ProxyServer::UNSPECIFIED_TYPE),
          false, std::vector<DataReductionProxyServer>(), 0,
      },
      {
          DataReductionProxyServer(
              net::ProxyServer::FromURI("origin2.net:443",
                                        net::ProxyServer::SCHEME_QUIC),
              ProxyServer::UNSPECIFIED_TYPE),
          false, std::vector<DataReductionProxyServer>(), 0,
      },
  };

  std::unique_ptr<DataReductionProxyMutableConfigValues> config_values =
      base::MakeUnique<DataReductionProxyMutableConfigValues>();

  config_values->UpdateValues(proxies_for_http);
  std::unique_ptr<DataReductionProxyConfig> config(new DataReductionProxyConfig(
      task_runner(), net_log(), std::move(config_values), configurator(),
      event_creator()));
  for (const auto& test : tests) {
    DataReductionProxyTypeInfo proxy_type_info;
    EXPECT_EQ(test.expected_result,
              config->IsDataReductionProxy(test.proxy_server.proxy_server(),
                                           &proxy_type_info));
    EXPECT_EQ(proxy_type_info.proxy_servers,
              DataReductionProxyServer::ConvertToNetProxyServers(
                  test.expected_proxies));
    EXPECT_EQ(test.expected_proxy_index, proxy_type_info.proxy_index);
  }
}

TEST_F(DataReductionProxyConfigTest, ShouldEnableServerPreviews) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataReductionProxyDecidesTransform);

  net::TestURLRequestContext context;
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request = context.CreateRequest(
      GURL(), net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->SetLoadFlags(request->load_flags() |
                        net::LOAD_MAIN_FRAME_DEPRECATED);
  std::unique_ptr<TestPreviewsDecider> previews_decider =
      base::MakeUnique<TestPreviewsDecider>(true);
  EXPECT_TRUE(test_config()->ShouldAcceptServerPreview(
      *request.get(), *previews_decider.get()));

  previews_decider = base::MakeUnique<TestPreviewsDecider>(false);
  EXPECT_FALSE(test_config()->ShouldAcceptServerPreview(
      *request.get(), *previews_decider.get()));
}

TEST_F(DataReductionProxyConfigTest, ShouldAcceptServerPreview) {
  // Turn on proxy-decides-transform feature to satisfy DCHECK.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDataReductionProxyDecidesTransform);
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(
      "DataReductionProxyPreviewsBlackListTransition", "Enabled");

  base::HistogramTester histogram_tester;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
      GURL(), net::IDLE, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->SetLoadFlags(request->load_flags() |
                        net::LOAD_MAIN_FRAME_DEPRECATED);
  std::unique_ptr<TestPreviewsDecider> previews_decider =
      base::MakeUnique<TestPreviewsDecider>(true);

  // Verify true for no flags.
  EXPECT_TRUE(test_config()->ShouldAcceptServerPreview(
      *request.get(), *previews_decider.get()));

  // Verify false for kill switch.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyLoFi,
      switches::kDataReductionProxyLoFiValueDisabled);
  EXPECT_FALSE(test_config()->ShouldAcceptServerPreview(
      *request.get(), *previews_decider.get()));
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.NotAcceptingTransform",
      0 /* NOT_ACCEPTING_TRANSFORM_DISABLED */, 1);

  // Verify true for Slow Connection flag.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyLoFi,
      switches::kDataReductionProxyLoFiValueSlowConnectionsOnly);
  EXPECT_TRUE(test_config()->ShouldAcceptServerPreview(
      *request.get(), *previews_decider.get()));

  // Verify false for Cellular Only flag and WIFI connection.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyLoFi,
      switches::kDataReductionProxyLoFiValueCellularOnly);
  test_config()->SetConnectionTypeForTesting(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(test_config()->ShouldAcceptServerPreview(
      *request.get(), *previews_decider.get()));
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.NotAcceptingTransform",
      2 /* NOT_ACCEPTING_TRANSFORM_CELLULAR_ONLY */, 1);

  // Verify true for Cellular Only flag and 3G connection.
  test_config()->SetConnectionTypeForTesting(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);
  EXPECT_TRUE(test_config()->ShouldAcceptServerPreview(
      *request.get(), *previews_decider.get()));

  // Verify PreviewsDecider check.
  previews_decider = base::MakeUnique<TestPreviewsDecider>(false);
  EXPECT_FALSE(test_config()->ShouldAcceptServerPreview(
      *request.get(), *previews_decider.get()));
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.NotAcceptingTransform",
      1 /* NOT_ACCEPTING_TRANSFORM_BLACKLISTED */, 1);
  previews_decider = base::MakeUnique<TestPreviewsDecider>(true);

  // Verfiy true for always on.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyLoFi,
      switches::kDataReductionProxyLoFiValueAlwaysOn);
  EXPECT_TRUE(test_config()->ShouldAcceptServerPreview(
      *request.get(), *previews_decider.get()));
}

TEST_F(DataReductionProxyConfigTest, HandleWarmupFetcherResponse) {
  base::HistogramTester histogram_tester;
  const net::URLRequestStatus kSuccess(net::URLRequestStatus::SUCCESS, net::OK);
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "fallback.net:80", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kNonDataSaverProxy = net::ProxyServer::FromURI(
      "https://non-data-saver-proxy.net:443", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings();

  // The proxy is enabled.
  test_config()->UpdateConfigForTesting(true, true, true);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  // Report failed warmup for a non-DataSaver proxy, and verify that it does not
  // change the list of data saver proxies.
  test_config()->HandleWarmupFetcherResponse(net::ProxyServer(),
                                             false /* success_response */);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  // Report successful warmup of |kHttpsProxy|.
  test_config()->HandleWarmupFetcherResponse(kHttpsProxy, true);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      1, 1);

  // Report failed warmup |kHttpsProxy| and verify it is removed from the list
  // of proxies.
  test_config()->HandleWarmupFetcherResponse(kHttpsProxy, false);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      0, 1);

  // Report failed warmup |kHttpsProxy| again, and verify it does not change the
  // list of proxies.
  test_config()->HandleWarmupFetcherResponse(kHttpsProxy, false);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      0, 2);

  // |kHttpsProxy| should now be added back to the list of proxies.
  test_config()->HandleWarmupFetcherResponse(kHttpsProxy, true);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      1, 2);

  // Report successful warmup |kHttpsProxy| again, and verify that there is no
  // change in the list of proxies..
  test_config()->HandleWarmupFetcherResponse(kHttpsProxy, true);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      1, 3);

  // |kHttpsProxy| should be removed again from the list of proxies.
  test_config()->HandleWarmupFetcherResponse(kHttpsProxy, false);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      0, 3);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      1, 3);

  // Now report failed warmup for |kHttpProxy| and verify that it is also
  // removed from the list of proxies.
  test_config()->HandleWarmupFetcherResponse(kHttpProxy, false);
  EXPECT_EQ(std::vector<net::ProxyServer>({}), GetConfiguredProxiesForHttp());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.NonCore",
      0, 1);

  // Both proxies should be added back.
  test_config()->HandleWarmupFetcherResponse(kHttpsProxy, true);
  test_config()->HandleWarmupFetcherResponse(kHttpProxy, true);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      0, 3);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      1, 4);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.NonCore",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.NonCore",
      1, 1);

  // If the warmup URL is unsuccessfully fetched using a non-data saver proxy,
  // then there is no change in the list of proxies.
  test_config()->HandleWarmupFetcherResponse(kNonDataSaverProxy, false);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
}

}  // namespace data_reduction_proxy

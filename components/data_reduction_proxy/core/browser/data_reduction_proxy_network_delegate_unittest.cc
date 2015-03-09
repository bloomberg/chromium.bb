// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "net/base/capturing_net_log.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {
namespace {

using TestNetworkDelegate = net::NetworkDelegateImpl;

const char kChromeProxyHeader[] = "chrome-proxy";

#if defined(OS_ANDROID)
const Client kClient = Client::CHROME_ANDROID;
#elif defined(OS_IOS)
const Client kClient = Client::CHROME_IOS;
#elif defined(OS_MACOSX)
const Client kClient = Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
const Client kClient = Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
const Client kClient = Client::CHROME_LINUX;
#elif defined(OS_WIN)
const Client kClient = Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
const Client kClient = Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
const Client kClient = Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
const Client kClient = Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
const Client kClient = Client::CHROME_QNX;
#else
const Client kClient = Client::UNKNOWN;
#endif

}  // namespace

class DataReductionProxyNetworkDelegateTest : public testing::Test {
 public:
  DataReductionProxyNetworkDelegateTest() : context_(true) {
    context_.Init();

    // The |test_job_factory_| takes ownership of the interceptor.
    test_job_interceptor_ = new net::TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(url::kHttpScheme,
                                                     test_job_interceptor_));
    context_.set_job_factory(&test_job_factory_);

    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed |
                                 DataReductionProxyParams::kFallbackAllowed |
                                 DataReductionProxyParams::kPromoAllowed)
            .WithParamsDefinitions(
                TestDataReductionProxyParams::HAS_EVERYTHING &
                    ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                    ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN)\
            .WithClient(kClient)
            .WithURLRequestContext(&context_)
            .Build();

    data_reduction_proxy_network_delegate_.reset(
        new DataReductionProxyNetworkDelegate(
            scoped_ptr<net::NetworkDelegate>(new TestNetworkDelegate()),
            config(), test_context_->io_data()->request_options(),
            test_context_->configurator()));
  }

  const net::ProxyConfig& GetProxyConfig() const {
    return config_;
  }

 protected:
  scoped_ptr<net::URLRequest> FetchURLRequest(
      const GURL& url,
      const std::string& raw_response_headers,
      int64 response_content_length) {
    scoped_ptr<net::URLRequest> request = context_.CreateRequest(
        url, net::IDLE, &delegate_, NULL);

    // Create a test job that will fill in the given response headers for the
    // |fake_request|.
    scoped_refptr<net::URLRequestTestJob> test_job(
        new net::URLRequestTestJob(request.get(),
                                   context_.network_delegate(),
                                   raw_response_headers, std::string(), true));

    // Configure the interceptor to use the test job to handle the next request.
    test_job_interceptor_->set_main_intercept_job(test_job.get());

    request->set_received_response_content_length(response_content_length);

    request->Start();
    test_context_->RunUntilIdle();

    if (!raw_response_headers.empty())
      EXPECT_TRUE(request->response_headers() != NULL);

    return request.Pass();
  }

  void set_network_delegate(net::NetworkDelegate* delegate) {
    network_delegate_ = delegate;
    context_.set_network_delegate(network_delegate_);
  }

  TestDataReductionProxyParams* params() const {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyIOData* io_data() const {
    return test_context_->io_data();
  }

  TestingPrefServiceSimple* pref_service() const {
    return test_context_->pref_service();
  }

  TestDataReductionProxyConfig* config() {
    return test_context_->config();
  }

  DataReductionProxyStatisticsPrefs* statistics_prefs() const {
    return test_context_->data_reduction_proxy_service()->statistics_prefs();
  }

  scoped_ptr<DataReductionProxyNetworkDelegate>
      data_reduction_proxy_network_delegate_;

 private:
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  // |test_job_interceptor_| is owned by |test_job_factory_|.
  net::TestJobInterceptor* test_job_interceptor_;
  net::URLRequestJobFactoryImpl test_job_factory_;

  net::ProxyConfig config_;
  net::NetworkDelegate* network_delegate_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyNetworkDelegateTest, AuthenticationTest) {
  set_network_delegate(data_reduction_proxy_network_delegate_.get());
  scoped_ptr<net::URLRequest> fake_request(
      FetchURLRequest(GURL("http://www.google.com/"), std::string(), 0));

  net::ProxyInfo data_reduction_proxy_info;
  std::string data_reduction_proxy;
  base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);

  net::HttpRequestHeaders headers;
  data_reduction_proxy_network_delegate_->NotifyBeforeSendProxyHeaders(
      fake_request.get(), data_reduction_proxy_info, &headers);

  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
  std::string header_value;
  headers.GetHeader(kChromeProxyHeader, &header_value);
  EXPECT_TRUE(header_value.find("ps=") != std::string::npos);
  EXPECT_TRUE(header_value.find("sid=") != std::string::npos);
}

TEST_F(DataReductionProxyNetworkDelegateTest, NetHistograms) {
  const std::string kReceivedValidOCLHistogramName =
      "Net.HttpContentLengthWithValidOCL";
  const std::string kOriginalValidOCLHistogramName =
      "Net.HttpOriginalContentLengthWithValidOCL";
  const std::string kDifferenceValidOCLHistogramName =
      "Net.HttpContentLengthDifferenceWithValidOCL";
  const std::string kReceivedHistogramName = "Net.HttpContentLength";
  const std::string kOriginalHistogramName = "Net.HttpOriginalContentLength";
  const std::string kDifferenceHistogramName =
      "Net.HttpContentLengthDifference";
  const std::string kFreshnessLifetimeHistogramName =
      "Net.HttpContentFreshnessLifetime";
  const std::string kCacheableHistogramName = "Net.HttpContentLengthCacheable";
  const std::string kCacheable4HoursHistogramName =
      "Net.HttpContentLengthCacheable4Hours";
  const std::string kCacheable24HoursHistogramName =
      "Net.HttpContentLengthCacheable24Hours";
  const int64 kResponseContentLength = 100;
  const int64 kOriginalContentLength = 200;

  base::HistogramTester histogram_tester;

  set_network_delegate(data_reduction_proxy_network_delegate_.get());

  std::string raw_headers =
      "HTTP/1.1 200 OK\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\n"
      "Via: 1.1 Chrome-Compression-Proxy\n"
      "x-original-content-length: " +
      base::Int64ToString(kOriginalContentLength) + "\n";

  HeadersToRaw(&raw_headers);

  scoped_ptr<net::URLRequest> fake_request(
      FetchURLRequest(GURL("http://www.google.com/"),
                      raw_headers, kResponseContentLength));

  base::TimeDelta freshness_lifetime =
      fake_request->response_info().headers->GetFreshnessLifetimes(
          fake_request->response_info().response_time).freshness;

  histogram_tester.ExpectUniqueSample(kReceivedValidOCLHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kOriginalValidOCLHistogramName,
                                      kOriginalContentLength, 1);
  histogram_tester.ExpectUniqueSample(
      kDifferenceValidOCLHistogramName,
      kOriginalContentLength - kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kReceivedHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kOriginalHistogramName,
                                      kOriginalContentLength, 1);
  histogram_tester.ExpectUniqueSample(
      kDifferenceHistogramName,
      kOriginalContentLength - kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kFreshnessLifetimeHistogramName,
                                      freshness_lifetime.InSeconds(), 1);
  histogram_tester.ExpectUniqueSample(kCacheableHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kCacheable4HoursHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kCacheable24HoursHistogramName,
                                      kResponseContentLength, 1);
}

class BadEntropyProvider : public base::FieldTrial::EntropyProvider {
 public:
  ~BadEntropyProvider() override {}

  double GetEntropyForTrial(const std::string& trial_name,
                            uint32 randomization_seed) const override {
    return 0.5;
  }
};

TEST_F(DataReductionProxyNetworkDelegateTest, OnResolveProxyHandler) {
  int load_flags = net::LOAD_NORMAL;
  GURL url("http://www.google.com/");

  // Data reduction proxy info
  net::ProxyInfo data_reduction_proxy_info;
  std::string data_reduction_proxy;
  base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UsePacString(
      "PROXY " +
      net::ProxyServer::FromURI(
          params()->DefaultOrigin(),
          net::ProxyServer::SCHEME_HTTP).host_port_pair().ToString() +
      "; DIRECT");
  EXPECT_FALSE(data_reduction_proxy_info.is_empty());

  // Data reduction proxy config
  net::ProxyConfig data_reduction_proxy_config;
  data_reduction_proxy_config.proxy_rules().ParseFromString(
      "http=" + data_reduction_proxy + ",direct://;");
  data_reduction_proxy_config.set_id(1);

  // Other proxy info
  net::ProxyInfo other_proxy_info;
  other_proxy_info.UseNamedProxy("proxy.com");
  EXPECT_FALSE(other_proxy_info.is_empty());

  // Direct
  net::ProxyInfo direct_proxy_info;
  direct_proxy_info.UseDirect();
  EXPECT_TRUE(direct_proxy_info.is_direct());

  // Empty retry info map
  net::ProxyRetryInfoMap empty_proxy_retry_info;

  // Retry info map with the data reduction proxy;
  net::ProxyRetryInfoMap data_reduction_proxy_retry_info;
  net::ProxyRetryInfo retry_info;
  retry_info.current_delay = base::TimeDelta::FromSeconds(1000);
  retry_info.bad_until = base::TimeTicks().Now() + retry_info.current_delay;
  retry_info.try_while_bad = false;
  data_reduction_proxy_retry_info[
     data_reduction_proxy_info.proxy_server().ToURI()] = retry_info;

  net::ProxyInfo result;
  // Another proxy is used. It should be used afterwards.
  result.Use(other_proxy_info);
  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &result);
  EXPECT_EQ(other_proxy_info.proxy_server(), result.proxy_server());

  // A direct connection is used. The data reduction proxy should be used
  // afterwards.
  // Another proxy is used. It should be used afterwards.
  result.Use(direct_proxy_info);
  net::ProxyConfig::ID prev_id = result.config_id();
  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &result);
  EXPECT_EQ(data_reduction_proxy_info.proxy_server(), result.proxy_server());
  // Only the proxy list should be updated, not he proxy info.
  EXPECT_EQ(result.config_id(), prev_id);

  // A direct connection is used, but the data reduction proxy is on the retry
  // list. A direct connection should be used afterwards.
  result.Use(direct_proxy_info);
  prev_id = result.config_id();
  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        data_reduction_proxy_retry_info, config(), &result);
  EXPECT_TRUE(result.proxy_server().is_direct());
  EXPECT_EQ(result.config_id(), prev_id);

  // Test that ws:// and wss:// URLs bypass the data reduction proxy.
  result.UseDirect();
  OnResolveProxyHandler(GURL("ws://echo.websocket.org/"),
                        load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &result);
  EXPECT_TRUE(result.is_direct());

  OnResolveProxyHandler(GURL("wss://echo.websocket.org/"),
                        load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &result);
  EXPECT_TRUE(result.is_direct());

  // Without DataCompressionProxyCriticalBypass Finch trial set, the
  // BYPASS_DATA_REDUCTION_PROXY load flag should be ignored.
  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(),
                        &result);
  EXPECT_FALSE(result.is_direct());

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info,
                        config(), &other_proxy_info);
  EXPECT_FALSE(other_proxy_info.is_direct());

  load_flags |= net::LOAD_BYPASS_DATA_REDUCTION_PROXY;

  result.UseDirect();
  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(),
                        &result);
  EXPECT_FALSE(result.is_direct());

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info,
                        config(), &other_proxy_info);
  EXPECT_FALSE(other_proxy_info.is_direct());

  // With Finch trial set, should only bypass if LOAD flag is set and the
  // effective proxy is the data compression proxy.
  base::FieldTrialList field_trial_list(new BadEntropyProvider());
  base::FieldTrialList::CreateFieldTrial("DataCompressionProxyCriticalBypass",
                                         "Enabled");
  EXPECT_TRUE(
      DataReductionProxyParams::IsIncludedInCriticalPathBypassFieldTrial());

  load_flags = net::LOAD_NORMAL;

  result.UseDirect();
  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(),
                        &result);
  EXPECT_FALSE(result.is_direct());

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(),
                        &other_proxy_info);
  EXPECT_FALSE(other_proxy_info.is_direct());

  load_flags |= net::LOAD_BYPASS_DATA_REDUCTION_PROXY;

  result.UseDirect();
  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(),
                        &result);
  EXPECT_TRUE(result.is_direct());

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(),
                        &other_proxy_info);
  EXPECT_FALSE(other_proxy_info.is_direct());
}

TEST_F(DataReductionProxyNetworkDelegateTest, TotalLengths) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;

  data_reduction_proxy_network_delegate_->data_reduction_proxy_io_data_ =
      io_data();

  data_reduction_proxy_network_delegate_->UpdateContentLengthPrefs(
      kReceivedLength, kOriginalLength,
      pref_service()->GetBoolean(
          data_reduction_proxy::prefs::kDataReductionProxyEnabled),
      UNKNOWN_TYPE);

  EXPECT_EQ(kReceivedLength,
            statistics_prefs()->GetInt64(
                data_reduction_proxy::prefs::kHttpReceivedContentLength));
  EXPECT_FALSE(pref_service()->GetBoolean(
      data_reduction_proxy::prefs::kDataReductionProxyEnabled));
  EXPECT_EQ(kOriginalLength,
            statistics_prefs()->GetInt64(
                data_reduction_proxy::prefs::kHttpOriginalContentLength));

  // Record the same numbers again, and total lengths should be doubled.
  data_reduction_proxy_network_delegate_->UpdateContentLengthPrefs(
      kReceivedLength, kOriginalLength,
      pref_service()->GetBoolean(
          data_reduction_proxy::prefs::kDataReductionProxyEnabled),
      UNKNOWN_TYPE);

  EXPECT_EQ(kReceivedLength * 2,
            statistics_prefs()->GetInt64(
                data_reduction_proxy::prefs::kHttpReceivedContentLength));
  EXPECT_FALSE(pref_service()->GetBoolean(
      data_reduction_proxy::prefs::kDataReductionProxyEnabled));
  EXPECT_EQ(kOriginalLength * 2,
            statistics_prefs()->GetInt64(
                data_reduction_proxy::prefs::kHttpOriginalContentLength));
}

}  // namespace data_reduction_proxy

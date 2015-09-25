// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
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
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
        url::kHttpScheme, make_scoped_ptr(test_job_interceptor_)));
    context_.set_job_factory(&test_job_factory_);

    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithClient(kClient)
                        .WithURLRequestContext(&context_)
                        .Build();

    data_reduction_proxy_network_delegate_.reset(
        new DataReductionProxyNetworkDelegate(
            scoped_ptr<net::NetworkDelegate>(new TestNetworkDelegate()),
            config(), test_context_->io_data()->request_options(),
            test_context_->configurator(),
            test_context_->io_data()->experiments_stats(),
            test_context_->net_log(), test_context_->event_creator()));

    bypass_stats_.reset(new DataReductionProxyBypassStats(
        config(), test_context_->unreachable_callback()));

    data_reduction_proxy_network_delegate_->InitIODataAndUMA(
        test_context_->io_data(), bypass_stats_.get());
  }

  const net::ProxyConfig& GetProxyConfig() const {
    return config_;
  }

  MockDataReductionProxyConfig* config() const {
    return test_context_->mock_config();
  }

  static void VerifyLoFiHeader(bool expected_lofi_used,
                               const net::HttpRequestHeaders& headers) {
    EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
    std::string header_value;
    headers.GetHeader(kChromeProxyHeader, &header_value);
    EXPECT_EQ(expected_lofi_used,
              header_value.find("q=low") != std::string::npos);
  }

  void VerifyWasLoFiModeActiveOnMainFrame(bool expected_value) {
    test_context_->RunUntilIdle();
    EXPECT_EQ(expected_value,
              test_context_->settings()->WasLoFiModeActiveOnMainFrame());
  }

  int64 total_received_bytes() {
    return data_reduction_proxy_network_delegate_->total_received_bytes_;
  }

  int64 total_original_received_bytes() {
    return data_reduction_proxy_network_delegate_
        ->total_original_received_bytes_;
  }

 protected:
  scoped_ptr<net::URLRequest> FetchURLRequest(
      const GURL& url,
      const std::string& raw_response_headers,
      int64 response_content_length) {
    scoped_ptr<net::URLRequest> request = context_.CreateRequest(
        url, net::IDLE, &delegate_);

    // Create a test job that will fill in the given response headers for the
    // |fake_request|.
    scoped_refptr<net::URLRequestTestJob> test_job(
        new net::URLRequestTestJob(request.get(),
                                   context_.network_delegate(),
                                   raw_response_headers, std::string(), true));

    // Configure the interceptor to use the test job to handle the next request.
    test_job_interceptor_->set_main_intercept_job(test_job.get());

    request->set_received_response_content_length(response_content_length);
    net::HttpResponseInfo& response_info =
        const_cast<net::HttpResponseInfo&>(request->response_info());
    response_info.network_accessed = true;

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

  TestDataReductionProxyConfig* config() {
    return test_context_->config();
  }

  scoped_ptr<DataReductionProxyNetworkDelegate>
      data_reduction_proxy_network_delegate_;

 private:
  base::MessageLoopForIO message_loop_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  // |test_job_interceptor_| is owned by |test_job_factory_|.
  net::TestJobInterceptor* test_job_interceptor_;
  net::URLRequestJobFactoryImpl test_job_factory_;

  net::ProxyConfig config_;
  net::NetworkDelegate* network_delegate_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
  scoped_ptr<DataReductionProxyBypassStats> bypass_stats_;
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

TEST_F(DataReductionProxyNetworkDelegateTest, LoFiTransitions) {
  set_network_delegate(data_reduction_proxy_network_delegate_.get());
  // Enable Lo-Fi.
  const struct {
    bool lofi_switch_enabled;
    bool auto_lofi_enabled;
  } tests[] = {
      {
       // Lo-Fi enabled through switch.
       false,
       true,
      },
      {
       // Lo-Fi enabled through field trial.
       true,
       false,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    if (tests[i].lofi_switch_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }
    config()->SetIncludedInLoFiEnabledFieldTrial(tests[i].auto_lofi_enabled);
    config()->SetNetworkProhibitivelySlow(tests[i].auto_lofi_enabled);

    net::ProxyInfo data_reduction_proxy_info;
    std::string data_reduction_proxy;
    base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
    data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);

    {
      // Main frame loaded. Lo-Fi should be used.
      net::HttpRequestHeaders headers;

      scoped_ptr<net::URLRequest> fake_request(
          FetchURLRequest(GURL("http://www.google.com/"), std::string(), 0));
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME);
      data_reduction_proxy_network_delegate_->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(true, headers);
      VerifyWasLoFiModeActiveOnMainFrame(true);
    }

    {
      // Bypass cache flag used. Lo-Fi should not be used.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(
          FetchURLRequest(GURL("http://www.google.com/"), std::string(), 0));
      fake_request->SetLoadFlags(net::LOAD_BYPASS_CACHE);
      data_reduction_proxy_network_delegate_->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(false, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // true.
      VerifyWasLoFiModeActiveOnMainFrame(true);
    }

    {
      // Bypass cache flag not used. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(
          FetchURLRequest(GURL("http://www.google.com/"), std::string(), 0));

      data_reduction_proxy_network_delegate_->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(true, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // true.
      VerifyWasLoFiModeActiveOnMainFrame(true);
    }

    {
      // Bypass cache flag used. Lo-Fi should not be used.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(
          FetchURLRequest(GURL("http://www.google.com/"), std::string(), 0));
      fake_request->SetLoadFlags(net::LOAD_BYPASS_CACHE);
      data_reduction_proxy_network_delegate_->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(false, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // true.
      VerifyWasLoFiModeActiveOnMainFrame(true);
    }

    {
      // Main frame request with bypass cache flag. Lo-Fi should not be used.
      // State of Lo-Fi should persist until next page load.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(
          FetchURLRequest(GURL("http://www.google.com/"), std::string(), 0));
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME | net::LOAD_BYPASS_CACHE);
      data_reduction_proxy_network_delegate_->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(false, headers);
      VerifyWasLoFiModeActiveOnMainFrame(false);
    }

    {
      // Bypass cache flag not used. Lo-Fi is still not used.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(
          FetchURLRequest(GURL("http://www.google.com/"), std::string(), 0));
      data_reduction_proxy_network_delegate_->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(false, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // false.
      VerifyWasLoFiModeActiveOnMainFrame(false);
    }

    {
      // Main frame request. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(
          FetchURLRequest(GURL("http://www.google.com/"), std::string(), 0));
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME);
      data_reduction_proxy_network_delegate_->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(true, headers);
      VerifyWasLoFiModeActiveOnMainFrame(true);
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest, NetHistograms) {
  const std::string kReceivedValidOCLHistogramName =
      "Net.HttpContentLengthWithValidOCL";
  const std::string kOriginalValidOCLHistogramName =
      "Net.HttpOriginalContentLengthWithValidOCL";
  const std::string kDifferenceValidOCLHistogramName =
      "Net.HttpContentLengthDifferenceWithValidOCL";

  // Lo-Fi histograms.
  const std::string kReceivedValidOCLLoFiOnHistogramName =
      "Net.HttpContentLengthWithValidOCL.LoFiOn";
  const std::string kOriginalValidOCLLoFiOnHistogramName =
      "Net.HttpOriginalContentLengthWithValidOCL.LoFiOn";
  const std::string kDifferenceValidOCLLoFiOnHistogramName =
      "Net.HttpContentLengthDifferenceWithValidOCL.LoFiOn";

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

  // Check Lo-Fi histograms.
  const struct {
    bool lofi_enabled_through_switch;
    bool auto_lofi_enabled;
    int expected_count;

  } tests[] = {
      {
       // Lo-Fi disabled.
       false,
       false,
       0,
      },
      {
       // Auto Lo-Fi enabled.
       // This should populate Lo-Fi content length histogram.
       false,
       true,
       1,
      },
      {
       // Lo-Fi enabled through switch.
       // This should populate Lo-Fi content length histogram.
       true,
       false,
       1,
      },
      {
       // Lo-Fi enabled through switch and Auto Lo-Fi also enabled.
       // This should populate Lo-Fi content length histogram.
       true,
       true,
       1,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    config()->ResetLoFiStatusForTest();
    config()->SetIncludedInLoFiEnabledFieldTrial(tests[i].auto_lofi_enabled);
    config()->SetNetworkProhibitivelySlow(tests[i].auto_lofi_enabled);

    if (tests[i].lofi_enabled_through_switch) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }

    config()->UpdateLoFiStatusOnMainFrameRequest(false, nullptr);

    fake_request = (FetchURLRequest(GURL("http://www.example.com/"),
                                    raw_headers, kResponseContentLength));

    // Histograms are accumulative, so get the sum of all the tests so far.
    int expected_count = 0;
    for (size_t j = 0; j <= i; ++j)
      expected_count += tests[j].expected_count;

    if (expected_count == 0) {
      histogram_tester.ExpectTotalCount(kReceivedValidOCLLoFiOnHistogramName,
                                        expected_count);
      histogram_tester.ExpectTotalCount(kOriginalValidOCLLoFiOnHistogramName,
                                        expected_count);
      histogram_tester.ExpectTotalCount(kDifferenceValidOCLLoFiOnHistogramName,
                                        expected_count);
    } else {
      histogram_tester.ExpectUniqueSample(kReceivedValidOCLLoFiOnHistogramName,
                                          kResponseContentLength,
                                          expected_count);
      histogram_tester.ExpectUniqueSample(kOriginalValidOCLLoFiOnHistogramName,
                                          kOriginalContentLength,
                                          expected_count);
      histogram_tester.ExpectUniqueSample(
          kDifferenceValidOCLLoFiOnHistogramName,
          kOriginalContentLength - kResponseContentLength, expected_count);
    }
  }
}

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
}

// Notify network delegate with a NULL request.
TEST_F(DataReductionProxyNetworkDelegateTest, NullRequest) {
  net::HttpRequestHeaders headers;
  net::ProxyInfo data_reduction_proxy_info;
  std::string data_reduction_proxy;
  base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UsePacString(
      "PROXY " +
      net::ProxyServer::FromURI(params()->DefaultOrigin(),
                                net::ProxyServer::SCHEME_HTTP)
          .host_port_pair()
          .ToString() +
      "; DIRECT");
  EXPECT_FALSE(data_reduction_proxy_info.is_empty());

  data_reduction_proxy_network_delegate_->NotifyBeforeSendProxyHeaders(
      nullptr, data_reduction_proxy_info, &headers);
  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
}

TEST_F(DataReductionProxyNetworkDelegateTest, OnCompletedInternal) {
  const int64 kResponseContentLength = 140;
  const int64 kOriginalContentLength = 200;

  set_network_delegate(data_reduction_proxy_network_delegate_.get());

  std::string raw_headers =
      "HTTP/1.1 200 OK\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\n"
      "Via: 1.1 Chrome-Compression-Proxy\n"
      "x-original-content-length: " +
      base::Int64ToString(kOriginalContentLength) + "\n";

  HeadersToRaw(&raw_headers);

  FetchURLRequest(GURL("http://www.google.com/"), raw_headers,
                  kResponseContentLength);

  EXPECT_EQ(kResponseContentLength, total_received_bytes());
  EXPECT_EQ(kOriginalContentLength + static_cast<long>(raw_headers.size()),
            total_original_received_bytes());
}

}  // namespace data_reduction_proxy

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
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

class TestLoFiDecider : public LoFiDecider {
 public:
  TestLoFiDecider() : should_request_lofi_resource_(false) {}
  ~TestLoFiDecider() override {}

  bool IsUsingLoFiMode(const net::URLRequest& request) const override {
    return should_request_lofi_resource_;
  }

  void SetIsUsingLoFiMode(bool should_request_lofi_resource) {
    should_request_lofi_resource_ = should_request_lofi_resource;
  }

  bool MaybeAddLoFiDirectiveToHeaders(
      const net::URLRequest& request,
      net::HttpRequestHeaders* headers) const override {
    if (should_request_lofi_resource_) {
      const char kChromeProxyHeader[] = "Chrome-Proxy";
      std::string header_value;

      if (headers->HasHeader(kChromeProxyHeader)) {
        headers->GetHeader(kChromeProxyHeader, &header_value);
        headers->RemoveHeader(kChromeProxyHeader);
        header_value += ", ";
      }

      header_value += "q=low";
      headers->SetHeader(kChromeProxyHeader, header_value);
      return true;
    }

    return false;
  }

 private:
  bool should_request_lofi_resource_;
};

class TestLoFiUIService : public LoFiUIService {
 public:
  TestLoFiUIService() : on_lofi_response_(false), is_preview_(false) {}
  ~TestLoFiUIService() override {}

  bool DidNotifyLoFiResponse() const { return on_lofi_response_; }
  bool is_preview() const { return is_preview_; }

  void OnLoFiReponseReceived(const net::URLRequest& request,
                             bool is_preview) override {
    on_lofi_response_ = true;
    is_preview_ = is_preview;
  }

 private:
  bool on_lofi_response_;
  bool is_preview_;
};

class DataReductionProxyNetworkDelegateTest : public testing::Test {
 public:
  DataReductionProxyNetworkDelegateTest()
      : context_(true),
        context_storage_(&context_),
        test_context_(DataReductionProxyTestContext::Builder()
                          .WithClient(kClient)
                          .WithMockClientSocketFactory(&mock_socket_factory_)
                          .WithURLRequestContext(&context_)
                          .Build()) {
    context_.set_client_socket_factory(&mock_socket_factory_);
    test_context_->AttachToURLRequestContext(&context_storage_);

    scoped_ptr<TestLoFiDecider> lofi_decider(new TestLoFiDecider());
    lofi_decider_ = lofi_decider.get();
    test_context_->io_data()->set_lofi_decider(std::move(lofi_decider));

    scoped_ptr<TestLoFiUIService> lofi_ui_service(new TestLoFiUIService());
    lofi_ui_service_ = lofi_ui_service.get();
    test_context_->io_data()->set_lofi_ui_service(std::move(lofi_ui_service));

    context_.Init();

    test_context_->EnableDataReductionProxyWithSecureProxyCheckSuccess();
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

  void VerifyDidNotifyLoFiResponse(bool lofi_response) const {
    EXPECT_EQ(lofi_response, lofi_ui_service_->DidNotifyLoFiResponse());
  }

  void VerifyLoFiPreviewResponse(bool is_preview) const {
    EXPECT_EQ(is_preview, lofi_ui_service_->is_preview());
  }

  // Each line in |response_headers| should end with "\r\n" and not '\0', and
  // the last line should have a second "\r\n".
  // An empty |response_headers| is allowed. It works by making this look like
  // an HTTP/0.9 response, since HTTP/0.9 responses don't have headers.
  scoped_ptr<net::URLRequest> FetchURLRequest(
      const GURL& url,
      net::HttpRequestHeaders* request_headers,
      const std::string& response_headers,
      int64_t response_content_length) {
    const std::string response_body(
        base::checked_cast<size_t>(response_content_length), ' ');
    net::MockRead reads[] = {net::MockRead(response_headers.c_str()),
                             net::MockRead(response_body.c_str()),
                             net::MockRead(net::SYNCHRONOUS, net::OK)};
    net::StaticSocketDataProvider socket(reads, arraysize(reads), nullptr, 0);
    mock_socket_factory_.AddSocketDataProvider(&socket);

    net::TestDelegate delegate;
    scoped_ptr<net::URLRequest> request =
        context_.CreateRequest(url, net::IDLE, &delegate);
    if (request_headers)
      request->SetExtraRequestHeaders(*request_headers);

    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

  int64_t total_received_bytes() const {
    return GetSessionNetworkStatsInfoInt64("session_received_content_length");
  }

  int64_t total_original_received_bytes() const {
    return GetSessionNetworkStatsInfoInt64("session_original_content_length");
  }

  net::MockClientSocketFactory* mock_socket_factory() {
    return &mock_socket_factory_;
  }

  net::TestURLRequestContext* context() { return &context_; }

  net::NetworkDelegate* network_delegate() const {
    return context_.network_delegate();
  }

  TestDataReductionProxyParams* params() const {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyConfig* config() const {
    return test_context_->config();
  }

  TestLoFiDecider* lofi_decider() const { return lofi_decider_; }

 private:
  int64_t GetSessionNetworkStatsInfoInt64(const char* key) const {
    const DataReductionProxyNetworkDelegate* drp_network_delegate =
        reinterpret_cast<const DataReductionProxyNetworkDelegate*>(
            context_.network_delegate());

    scoped_ptr<base::DictionaryValue> session_network_stats_info =
        base::DictionaryValue::From(make_scoped_ptr(
            drp_network_delegate->SessionNetworkStatsInfoToValue()));
    EXPECT_TRUE(session_network_stats_info);

    std::string string_value;
    EXPECT_TRUE(session_network_stats_info->GetString(key, &string_value));
    int64_t value = 0;
    EXPECT_TRUE(base::StringToInt64(string_value, &value));
    return value;
  }

  base::MessageLoopForIO message_loop_;
  net::MockClientSocketFactory mock_socket_factory_;
  net::TestURLRequestContext context_;
  net::URLRequestContextStorage context_storage_;

  TestLoFiDecider* lofi_decider_;
  TestLoFiUIService* lofi_ui_service_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyNetworkDelegateTest, AuthenticationTest) {
  scoped_ptr<net::URLRequest> fake_request(FetchURLRequest(
      GURL("http://www.google.com/"), nullptr, std::string(), 0));

  net::ProxyInfo data_reduction_proxy_info;
  std::string data_reduction_proxy;
  base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);

  net::HttpRequestHeaders headers;
  network_delegate()->NotifyBeforeSendProxyHeaders(
      fake_request.get(), data_reduction_proxy_info, &headers);

  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
  std::string header_value;
  headers.GetHeader(kChromeProxyHeader, &header_value);
  EXPECT_TRUE(header_value.find("ps=") != std::string::npos);
  EXPECT_TRUE(header_value.find("sid=") != std::string::npos);
}

TEST_F(DataReductionProxyNetworkDelegateTest, LoFiTransitions) {
  // Enable Lo-Fi.
  const struct {
    bool lofi_switch_enabled;
    bool auto_lofi_enabled;
  } tests[] = {
      {
          // Lo-Fi enabled through switch.
          true, false,
      },
      {
          // Lo-Fi enabled through field trial.
          false, true,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    if (tests[i].lofi_switch_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }
    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }
    config()->SetNetworkProhibitivelySlow(tests[i].auto_lofi_enabled);

    net::ProxyInfo data_reduction_proxy_info;
    std::string data_reduction_proxy;
    base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
    data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);

    {
      // Main frame loaded. Lo-Fi should be used.
      net::HttpRequestHeaders headers;

      scoped_ptr<net::URLRequest> fake_request(FetchURLRequest(
          GURL("http://www.google.com/"), nullptr, std::string(), 0));
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME);
      lofi_decider()->SetIsUsingLoFiMode(
          config()->ShouldEnableLoFiMode(*fake_request.get()));
      network_delegate()->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(true, headers);
      VerifyWasLoFiModeActiveOnMainFrame(true);
    }

    {
      // Lo-Fi is already off. Lo-Fi should not be used.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(FetchURLRequest(
          GURL("http://www.google.com/"), nullptr, std::string(), 0));
      lofi_decider()->SetIsUsingLoFiMode(false);
      network_delegate()->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(false, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // true.
      VerifyWasLoFiModeActiveOnMainFrame(true);
    }

    {
      // Lo-Fi is already on. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(FetchURLRequest(
          GURL("http://www.google.com/"), nullptr, std::string(), 0));

      lofi_decider()->SetIsUsingLoFiMode(true);
      network_delegate()->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(true, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // true.
      VerifyWasLoFiModeActiveOnMainFrame(true);
    }

    {
      // TODO(megjablon): Can remove the cases below once
      // WasLoFiModeActiveOnMainFrame is fixed to be per-page.
      // Main frame request with Lo-Fi off. Lo-Fi should not be used.
      // State of Lo-Fi should persist until next page load.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(FetchURLRequest(
          GURL("http://www.google.com/"), nullptr, std::string(), 0));
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME);
      lofi_decider()->SetIsUsingLoFiMode(false);
      network_delegate()->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(false, headers);
      VerifyWasLoFiModeActiveOnMainFrame(false);
    }

    {
      // Lo-Fi is off. Lo-Fi is still not used.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(FetchURLRequest(
          GURL("http://www.google.com/"), nullptr, std::string(), 0));
      lofi_decider()->SetIsUsingLoFiMode(false);
      network_delegate()->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(false, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // false.
      VerifyWasLoFiModeActiveOnMainFrame(false);
    }

    {
      // Main frame request. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      scoped_ptr<net::URLRequest> fake_request(FetchURLRequest(
          GURL("http://www.google.com/"), nullptr, std::string(), 0));
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME);
      lofi_decider()->SetIsUsingLoFiMode(
          config()->ShouldEnableLoFiMode(*fake_request.get()));
      network_delegate()->NotifyBeforeSendProxyHeaders(
          fake_request.get(), data_reduction_proxy_info, &headers);
      VerifyLoFiHeader(true, headers);
      VerifyWasLoFiModeActiveOnMainFrame(true);
    }
  }
}

#if defined(OS_ANDROID)
#define MAYBE_NetHistograms DISABLED_NetHistograms
#else
#define MAYBE_NetHistograms NetHistograms
#endif
TEST_F(DataReductionProxyNetworkDelegateTest, MAYBE_NetHistograms) {
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
  const int64_t kResponseContentLength = 100;
  const int64_t kOriginalContentLength = 200;

  base::HistogramTester histogram_tester;

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: " +
      base::Int64ToString(kOriginalContentLength) + "\r\n\r\n";

  scoped_ptr<net::URLRequest> fake_request(
      FetchURLRequest(GURL("http://www.google.com/"), nullptr, response_headers,
                      kResponseContentLength));

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
          false, false, 0,
      },
      {
          // Auto Lo-Fi enabled.
          // This should populate Lo-Fi content length histogram.
          false, true, 1,
      },
      {
          // Lo-Fi enabled through switch.
          // This should populate Lo-Fi content length histogram.
          true, false, 1,
      },
      {
          // Lo-Fi enabled through switch and Auto Lo-Fi also enabled.
          // This should populate Lo-Fi content length histogram.
          true, true, 1,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    config()->ResetLoFiStatusForTest();
    config()->SetNetworkProhibitivelySlow(tests[i].auto_lofi_enabled);
    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }

    if (tests[i].lofi_enabled_through_switch) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }

    lofi_decider()->SetIsUsingLoFiMode(
        config()->ShouldEnableLoFiMode(*fake_request.get()));

    fake_request = (FetchURLRequest(GURL("http://www.example.com/"), nullptr,
                                    response_headers, kResponseContentLength));

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
  data_reduction_proxy_retry_info[data_reduction_proxy_info.proxy_server()
                                      .ToURI()] = retry_info;

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
                        empty_proxy_retry_info, config(), &result);
  EXPECT_FALSE(result.is_direct());

  OnResolveProxyHandler(url, load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &other_proxy_info);
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

  network_delegate()->NotifyBeforeSendProxyHeaders(
      nullptr, data_reduction_proxy_info, &headers);
  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
}

#if defined(OS_ANDROID)
#define MAYBE_OnCompletedInternalLoFi DISABLED_OnCompletedInternalLoFi
#else
#define MAYBE_OnCompletedInternalLoFi OnCompletedInternalLoFi
#endif
TEST_F(DataReductionProxyNetworkDelegateTest, MAYBE_OnCompletedInternalLoFi) {
  // Enable Lo-Fi.
  const struct {
    bool lofi_response;
  } tests[] = {
      {false}, {true},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string response_headers =
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
        "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
        "Via: 1.1 Chrome-Compression-Proxy\r\n"
        "x-original-content-length: 200\r\n";

    if (tests[i].lofi_response)
      response_headers += "Chrome-Proxy: q=low\r\n";

    response_headers += "\r\n";
    FetchURLRequest(GURL("http://www.google.com/"), nullptr, response_headers,
                    140);

    VerifyDidNotifyLoFiResponse(tests[i].lofi_response);
  }
}

#if defined(OS_ANDROID)
#define MAYBE_OnCompletedInternalLoFiPreview \
  DISABLED_OnCompletedInternalLoFiPreview
#else
#define MAYBE_OnCompletedInternalLoFiPreview OnCompletedInternalLoFiPreview
#endif
TEST_F(DataReductionProxyNetworkDelegateTest,
       MAYBE_OnCompletedInternalLoFiPreview) {
  // Enable Lo-Fi.
  const struct {
    bool is_preview;
  } tests[] = {
      {false}, {true},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string response_headers =
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
        "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
        "Via: 1.1 Chrome-Compression-Proxy\r\n"
        "x-original-content-length: 200\r\n";

    if (tests[i].is_preview)
      response_headers += "Chrome-Proxy: q=preview\r\n";

    response_headers += "\r\n";
    FetchURLRequest(GURL("http://www.google.com/"), nullptr, response_headers,
                    140);

    VerifyDidNotifyLoFiResponse(tests[i].is_preview);
    VerifyLoFiPreviewResponse(tests[i].is_preview);
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       TestLoFiTransformationTypeHistogram) {
  const char kLoFiTransformationTypeHistogram[] =
      "DataReductionProxy.LoFi.TransformationType";
  base::HistogramTester histogram_tester;

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader("Chrome-Proxy", "q=preview");
  FetchURLRequest(GURL("http://www.google.com/"), &request_headers,
                  std::string(), 140);
  histogram_tester.ExpectBucketCount(kLoFiTransformationTypeHistogram,
                                     NO_TRANSFORMATION_PREVIEW_REQUESTED, 1);

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Chrome-Proxy: q=preview\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: 200\r\n";

  response_headers += "\r\n";
  FetchURLRequest(GURL("http://www.google.com/"), nullptr, response_headers,
                  140);

  histogram_tester.ExpectBucketCount(kLoFiTransformationTypeHistogram, PREVIEW,
                                     1);
}

TEST_F(DataReductionProxyNetworkDelegateTest, OnCompletedSizeFor200) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  const char kDrpResponseHeaders[] =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Warning: 199 Misc-Agent \"some warning text\"\r\n"
      "Via:\r\n"
      "Via: 1.1 Chrome-Compression-Proxy-Suffix, 9.9 other-proxy\r\n"
      "Via: 2.2 Chrome-Compression-Proxy\r\n"
      "Warning: 214 Chrome-Compression-Proxy \"Transformation Applied\"\r\n"
      "X-Original-Content-Length: 10000\r\n"
      "Chrome-Proxy: q=low\r\n"
      "Content-Length: 1000\r\n\r\n";

  scoped_ptr<net::URLRequest> request = FetchURLRequest(
      GURL("http://example.com/path/"), nullptr, kDrpResponseHeaders, 1000);

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);

  const std::string raw_headers = net::HttpUtil::AssembleRawHeaders(
      kDrpResponseHeaders, arraysize(kDrpResponseHeaders) - 1);
  EXPECT_EQ(static_cast<int64_t>(raw_headers.size() +
                                 10000 /* original_response_body */),
            total_original_received_bytes() - baseline_original_received_bytes);
}

TEST_F(DataReductionProxyNetworkDelegateTest, OnCompletedSizeFor304) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  const char kDrpResponseHeaders[] =
      "HTTP/1.1 304 Not Modified\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "X-Original-Content-Length: 10000\r\n\r\n";

  scoped_ptr<net::URLRequest> request = FetchURLRequest(
      GURL("http://example.com/path/"), nullptr, kDrpResponseHeaders, 0);

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);

  const std::string raw_headers = net::HttpUtil::AssembleRawHeaders(
      kDrpResponseHeaders, arraysize(kDrpResponseHeaders) - 1);
  EXPECT_EQ(static_cast<int64_t>(raw_headers.size() +
                                 10000 /* original_response_body */),
            total_original_received_bytes() - baseline_original_received_bytes);
}

TEST_F(DataReductionProxyNetworkDelegateTest, OnCompletedSizeForWriteError) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  net::MockWrite writes[] = {
      net::MockWrite("GET http://example.com/path/ HTTP/1.1\r\n"
                     "Host: example.com\r\n"),
      net::MockWrite(net::ASYNC, net::ERR_ABORTED)};
  net::StaticSocketDataProvider socket(nullptr, 0, writes, arraysize(writes));
  mock_socket_factory()->AddSocketDataProvider(&socket);

  net::TestDelegate delegate;
  scoped_ptr<net::URLRequest> request = context()->CreateRequest(
      GURL("http://example.com/path/"), net::IDLE, &delegate);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);
  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_original_received_bytes() - baseline_original_received_bytes);
}

TEST_F(DataReductionProxyNetworkDelegateTest, OnCompletedSizeForReadError) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 OK\r\n"
                                         "Via: 1.1 Chrome-Compression-Proxy\r\n"
                                         "X-Original-Content-Length: 10000\r\n"
                                         "Content-Length: 1000\r\n\r\n"),
                           net::MockRead(net::ASYNC, net::ERR_ABORTED)};
  net::StaticSocketDataProvider socket(reads, arraysize(reads), nullptr, 0);
  mock_socket_factory()->AddSocketDataProvider(&socket);

  net::TestDelegate delegate;
  scoped_ptr<net::URLRequest> request = context()->CreateRequest(
      GURL("http://example.com/path/"), net::IDLE, &delegate);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);
  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_original_received_bytes() - baseline_original_received_bytes);
}

}  // namespace

}  // namespace data_reduction_proxy

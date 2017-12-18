// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/warmup_url_fetcher.h"

#include <map>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "net/http/http_status_code.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class WarmupURLFetcherTest : public WarmupURLFetcher {
 public:
  WarmupURLFetcherTest(const scoped_refptr<net::URLRequestContextGetter>&
                           url_request_context_getter)
      : WarmupURLFetcher(url_request_context_getter,
                         base::BindRepeating(
                             &WarmupURLFetcherTest::HandleWarmupFetcherResponse,
                             base::Unretained(this))) {}

  ~WarmupURLFetcherTest() override {}

  size_t callback_received_count() const { return callback_received_count_; }
  const net::ProxyServer& proxy_server_last() const {
    return proxy_server_last_;
  }
  bool success_response_last() const { return success_response_last_; }

  static void InitExperiment(
      base::test::ScopedFeatureList* scoped_feature_list) {
    std::map<std::string, std::string> params;
    params["warmup_fetch_callback_enabled"] = "true";
    scoped_feature_list->InitAndEnableFeatureWithParameters(
        features::kDataReductionProxyRobustConnection, params);
  }

  using WarmupURLFetcher::FetchWarmupURL;
  using WarmupURLFetcher::GetWarmupURLWithQueryParam;

 private:
  void HandleWarmupFetcherResponse(const net::ProxyServer& proxy_server,
                                   bool success_response) {
    callback_received_count_++;
    proxy_server_last_ = proxy_server;
    success_response_last_ = success_response;
  }

  size_t callback_received_count_ = 0;
  net::ProxyServer proxy_server_last_;
  bool success_response_last_ = false;
  DISALLOW_COPY_AND_ASSIGN(WarmupURLFetcherTest);
};

// Test that query param for the warmup URL is randomly set.
TEST(WarmupURLFetcherTest, TestGetWarmupURLWithQueryParam) {
  base::MessageLoopForIO message_loop;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter =
      new net::TestURLRequestContextGetter(message_loop.task_runner());
  WarmupURLFetcherTest warmup_url_fetcher(request_context_getter);

  GURL gurl_original;
  warmup_url_fetcher.GetWarmupURLWithQueryParam(&gurl_original);
  EXPECT_FALSE(gurl_original.query().empty());

  bool query_param_different = false;

  // Generate 5 more GURLs. At least one of them should have a different query
  // param than that of |gurl_original|. Multiple GURLs are generated to
  // probability of test failing due to query params of two GURLs being equal
  // due to chance.
  for (size_t i = 0; i < 5; ++i) {
    GURL gurl;
    warmup_url_fetcher.GetWarmupURLWithQueryParam(&gurl);
    EXPECT_EQ(gurl_original.host(), gurl.host());
    EXPECT_EQ(gurl_original.port(), gurl.port());
    EXPECT_EQ(gurl_original.path(), gurl.path());

    EXPECT_FALSE(gurl.query().empty());

    if (gurl_original.query() != gurl.query())
      query_param_different = true;
  }
  EXPECT_TRUE(query_param_different);
}

TEST(WarmupURLFetcherTest, TestSuccessfulFetchWarmupURLNoViaHeader) {
  base::test::ScopedFeatureList scoped_feature_list;
  WarmupURLFetcherTest::InitExperiment(&scoped_feature_list);

  base::HistogramTester histogram_tester;
  base::MessageLoopForIO message_loop;
  const std::string config = "foobarbaz";
  std::vector<std::unique_ptr<net::SocketDataProvider>> socket_data_providers;
  net::MockClientSocketFactory mock_socket_factory;
  net::MockRead success_reads[3];
  success_reads[0] = net::MockRead("HTTP/1.1 200 OK\r\n\r\n");
  success_reads[1] = net::MockRead(net::ASYNC, config.c_str(), config.length());
  success_reads[2] = net::MockRead(net::SYNCHRONOUS, net::OK);

  socket_data_providers.push_back(
      (base::MakeUnique<net::StaticSocketDataProvider>(
          success_reads, arraysize(success_reads), nullptr, 0)));
  mock_socket_factory.AddSocketDataProvider(socket_data_providers.back().get());

  std::unique_ptr<net::TestURLRequestContext> test_request_context(
      new net::TestURLRequestContext(true));

  test_request_context->set_client_socket_factory(&mock_socket_factory);
  test_request_context->Init();
  scoped_refptr<net::URLRequestContextGetter> request_context_getter =
      new net::TestURLRequestContextGetter(message_loop.task_runner(),
                                           std::move(test_request_context));

  WarmupURLFetcherTest warmup_url_fetcher(request_context_getter);
  warmup_url_fetcher.FetchWarmupURL();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 1, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HttpResponseCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HasViaHeader", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.ProxySchemeUsed",
      util::ConvertNetProxySchemeToProxyScheme(net::ProxyServer::SCHEME_DIRECT),
      1);

  EXPECT_EQ(1u, warmup_url_fetcher.callback_received_count());
  EXPECT_EQ(net::ProxyServer::SCHEME_DIRECT,
            warmup_url_fetcher.proxy_server_last().scheme());
  // success_response_last() should be false since the response does not contain
  // the via header.
  EXPECT_FALSE(warmup_url_fetcher.success_response_last());
}

TEST(WarmupURLFetcherTest, TestSuccessfulFetchWarmupURLWithViaHeader) {
  base::test::ScopedFeatureList scoped_feature_list;
  WarmupURLFetcherTest::InitExperiment(&scoped_feature_list);

  base::HistogramTester histogram_tester;
  base::MessageLoopForIO message_loop;
  const std::string config = "foobarbaz";
  std::vector<std::unique_ptr<net::SocketDataProvider>> socket_data_providers;
  net::MockClientSocketFactory mock_socket_factory;
  net::MockRead success_reads[3];
  success_reads[0] = net::MockRead(
      "HTTP/1.1 204 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n");
  success_reads[1] = net::MockRead(net::ASYNC, config.c_str(), config.length());
  success_reads[2] = net::MockRead(net::SYNCHRONOUS, net::OK);

  socket_data_providers.push_back(
      (base::MakeUnique<net::StaticSocketDataProvider>(
          success_reads, arraysize(success_reads), nullptr, 0)));
  mock_socket_factory.AddSocketDataProvider(socket_data_providers.back().get());

  std::unique_ptr<net::TestURLRequestContext> test_request_context(
      new net::TestURLRequestContext(true));

  test_request_context->set_client_socket_factory(&mock_socket_factory);
  test_request_context->Init();
  scoped_refptr<net::URLRequestContextGetter> request_context_getter =
      new net::TestURLRequestContextGetter(message_loop.task_runner(),
                                           std::move(test_request_context));

  WarmupURLFetcherTest warmup_url_fetcher(request_context_getter);
  warmup_url_fetcher.FetchWarmupURL();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 1, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HttpResponseCode", net::HTTP_NO_CONTENT, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HasViaHeader", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.ProxySchemeUsed",
      util::ConvertNetProxySchemeToProxyScheme(net::ProxyServer::SCHEME_DIRECT),
      1);

  EXPECT_EQ(1u, warmup_url_fetcher.callback_received_count());
  EXPECT_EQ(net::ProxyServer::SCHEME_DIRECT,
            warmup_url_fetcher.proxy_server_last().scheme());
  // success_response_last() should be true since the response contains the via
  // header.
  EXPECT_TRUE(warmup_url_fetcher.success_response_last());
}

TEST(WarmupURLFetcherTest,
     TestSuccessfulFetchWarmupURLWithViaHeaderExperimentNotEnabled) {
  base::HistogramTester histogram_tester;
  base::MessageLoopForIO message_loop;
  const std::string config = "foobarbaz";
  std::vector<std::unique_ptr<net::SocketDataProvider>> socket_data_providers;
  net::MockClientSocketFactory mock_socket_factory;
  net::MockRead success_reads[3];
  success_reads[0] = net::MockRead(
      "HTTP/1.1 204 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n");
  success_reads[1] = net::MockRead(net::ASYNC, config.c_str(), config.length());
  success_reads[2] = net::MockRead(net::SYNCHRONOUS, net::OK);

  socket_data_providers.push_back(
      (base::MakeUnique<net::StaticSocketDataProvider>(
          success_reads, arraysize(success_reads), nullptr, 0)));
  mock_socket_factory.AddSocketDataProvider(socket_data_providers.back().get());

  std::unique_ptr<net::TestURLRequestContext> test_request_context(
      new net::TestURLRequestContext(true));

  test_request_context->set_client_socket_factory(&mock_socket_factory);
  test_request_context->Init();
  scoped_refptr<net::URLRequestContextGetter> request_context_getter =
      new net::TestURLRequestContextGetter(message_loop.task_runner(),
                                           std::move(test_request_context));

  WarmupURLFetcherTest warmup_url_fetcher(request_context_getter);
  warmup_url_fetcher.FetchWarmupURL();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 1, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HttpResponseCode", net::HTTP_NO_CONTENT, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HasViaHeader", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.ProxySchemeUsed",
      util::ConvertNetProxySchemeToProxyScheme(net::ProxyServer::SCHEME_DIRECT),
      1);

  // The callback should not be run.
  EXPECT_EQ(0u, warmup_url_fetcher.callback_received_count());
}

TEST(WarmupURLFetcherTest, TestConnectionResetFetchWarmupURL) {
  base::test::ScopedFeatureList scoped_feature_list;
  WarmupURLFetcherTest::InitExperiment(&scoped_feature_list);

  base::HistogramTester histogram_tester;
  base::MessageLoopForIO message_loop;
  const std::string config = "foobarbaz";
  std::vector<std::unique_ptr<net::SocketDataProvider>> socket_data_providers;
  net::MockClientSocketFactory mock_socket_factory;
  net::MockRead success_reads[1];
  success_reads[0] = net::MockRead(net::SYNCHRONOUS, net::ERR_CONNECTION_RESET);

  socket_data_providers.push_back(
      (base::MakeUnique<net::StaticSocketDataProvider>(
          success_reads, arraysize(success_reads), nullptr, 0)));
  mock_socket_factory.AddSocketDataProvider(socket_data_providers.back().get());

  std::unique_ptr<net::TestURLRequestContext> test_request_context(
      new net::TestURLRequestContext(true));

  test_request_context->set_client_socket_factory(&mock_socket_factory);
  test_request_context->Init();
  scoped_refptr<net::URLRequestContextGetter> request_context_getter =
      new net::TestURLRequestContextGetter(message_loop.task_runner(),
                                           std::move(test_request_context));

  WarmupURLFetcherTest warmup_url_fetcher(request_context_getter);
  warmup_url_fetcher.FetchWarmupURL();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 0, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      std::abs(net::ERR_CONNECTION_RESET), 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HttpResponseCode",
      std::abs(net::URLFetcher::RESPONSE_CODE_INVALID), 1);
  histogram_tester.ExpectTotalCount("DataReductionProxy.WarmupURL.HasViaHeader",
                                    0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.ProxySchemeUsed", 0);
  EXPECT_EQ(1u, warmup_url_fetcher.callback_received_count());
  EXPECT_EQ(net::ProxyServer::SCHEME_INVALID,
            warmup_url_fetcher.proxy_server_last().scheme());
  EXPECT_FALSE(warmup_url_fetcher.success_response_last());
}

}  // namespace

}  // namespace data_reduction_proxy
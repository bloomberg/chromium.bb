// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/warmup_url_fetcher.h"

#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
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
      : WarmupURLFetcher(url_request_context_getter) {}

  ~WarmupURLFetcherTest() override {}

  using WarmupURLFetcher::FetchWarmupURL;
  using WarmupURLFetcher::GetWarmupURLWithQueryParam;

 private:
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

TEST(WarmupURLFetcherTest, TestSuccessfulFetchWarmupURL) {
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
}

TEST(WarmupURLFetcherTest, TestConnectionResetFetchWarmupURL) {
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
}

}  // namespace

}  // namespace data_reduction_proxy
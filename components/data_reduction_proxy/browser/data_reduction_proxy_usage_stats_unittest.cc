// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/test/histogram_tester.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace {

class DataReductionProxyParamsMock :
    public data_reduction_proxy::DataReductionProxyParams {
 public:
  DataReductionProxyParamsMock() :
      data_reduction_proxy::DataReductionProxyParams(0) {}
  virtual ~DataReductionProxyParamsMock() {}

  MOCK_CONST_METHOD2(
      IsDataReductionProxy,
      bool(const net::HostPortPair& host_port_pair,
           data_reduction_proxy::DataReductionProxyTypeInfo* proxy_info));
  MOCK_CONST_METHOD2(
      WasDataReductionProxyUsed,
      bool(const net::URLRequest*,
           data_reduction_proxy::DataReductionProxyTypeInfo* proxy_info));

 private:
  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyParamsMock);
};

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyUsageStatsTest : public testing::Test {
 public:
  DataReductionProxyUsageStatsTest()
      : loop_proxy_(base::MessageLoopProxy::current().get()),
        context_(true),
        unavailable_(false) {
    context_.Init();

    // The |test_job_factory_| takes ownership of the interceptor.
    test_job_interceptor_ = new net::TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(url::kHttpScheme,
                                                     test_job_interceptor_));

    context_.set_job_factory(&test_job_factory_);

    mock_url_request_ = context_.CreateRequest(GURL(), net::IDLE, &delegate_,
                                               NULL);
  }

  void NotifyUnavailable(bool unavailable) {
    unavailable_ = unavailable;
  }

  scoped_ptr<net::URLRequest> CreateURLRequestWithResponseHeaders(
      const GURL& url,
      const std::string& raw_response_headers) {
    scoped_ptr<net::URLRequest> fake_request = context_.CreateRequest(
        url, net::IDLE, &delegate_, NULL);

    // Create a test job that will fill in the given response headers for the
    // |fake_request|.
    scoped_refptr<net::URLRequestTestJob> test_job(
        new net::URLRequestTestJob(fake_request.get(),
                                   context_.network_delegate(),
                                   raw_response_headers, std::string(), true));

    // Configure the interceptor to use the test job to handle the next request.
    test_job_interceptor_->set_main_intercept_job(test_job.get());
    fake_request->Start();
    base::MessageLoop::current()->RunUntilIdle();

    EXPECT_TRUE(fake_request->response_headers() != NULL);
    return fake_request.Pass();
  }

  // Required for base::MessageLoopProxy::current().
  base::MessageLoopForUI loop_;
  base::MessageLoopProxy* loop_proxy_;

 protected:
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  DataReductionProxyParamsMock mock_params_;
  scoped_ptr<net::URLRequest> mock_url_request_;
  // |test_job_interceptor_| is owned by |test_job_factory_|.
  net::TestJobInterceptor* test_job_interceptor_;
  net::URLRequestJobFactoryImpl test_job_factory_;
  bool unavailable_;
};

TEST_F(DataReductionProxyUsageStatsTest, IsDataReductionProxyUnreachable) {
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("foo.com", net::ProxyServer::SCHEME_HTTP);
  data_reduction_proxy::DataReductionProxyTypeInfo proxy_info;
  struct TestCase {
    bool fallback_proxy_server_is_data_reduction_proxy;
    bool was_proxy_used;
    bool is_unreachable;
  };
  const TestCase test_cases[] = {
    {
      false,
      false,
      false
    },
    {
      false,
      true,
      false
    },
    {
      true,
      true,
      false
    },
    {
      true,
      false,
      true
    }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    TestCase test_case = test_cases[i];

    EXPECT_CALL(mock_params_, IsDataReductionProxy(testing::_, testing::_))
        .WillRepeatedly(testing::Return(
            test_case.fallback_proxy_server_is_data_reduction_proxy));
    EXPECT_CALL(mock_params_,
                WasDataReductionProxyUsed(mock_url_request_.get(), NULL))
        .WillRepeatedly(testing::Return(test_case.was_proxy_used));

    scoped_ptr<DataReductionProxyUsageStats> usage_stats(
        new DataReductionProxyUsageStats(
            &mock_params_, loop_proxy_));
    usage_stats->set_unavailable_callback(
        base::Bind(&DataReductionProxyUsageStatsTest::NotifyUnavailable,
                   base::Unretained(this)));

    usage_stats->OnProxyFallback(fallback_proxy_server,
                                 net::ERR_PROXY_CONNECTION_FAILED);
    usage_stats->OnUrlRequestCompleted(mock_url_request_.get(), false);
    base::MessageLoop::current()->RunUntilIdle();

    EXPECT_EQ(test_case.is_unreachable, unavailable_);
  }
}

TEST_F(DataReductionProxyUsageStatsTest, ProxyUnreachableThenReachable) {
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("foo.com", net::ProxyServer::SCHEME_HTTP);
  scoped_ptr<DataReductionProxyUsageStats> usage_stats(
      new DataReductionProxyUsageStats(
          &mock_params_, loop_proxy_));
  usage_stats->set_unavailable_callback(
      base::Bind(&DataReductionProxyUsageStatsTest::NotifyUnavailable,
                 base::Unretained(this)));

  EXPECT_CALL(mock_params_, IsDataReductionProxy(testing::_, testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_params_,
              WasDataReductionProxyUsed(mock_url_request_.get(), NULL))
      .WillOnce(testing::Return(true));

  // proxy falls back
  usage_stats->OnProxyFallback(fallback_proxy_server,
                               net::ERR_PROXY_CONNECTION_FAILED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(unavailable_);

  // proxy succeeds
  usage_stats->OnUrlRequestCompleted(mock_url_request_.get(), false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(unavailable_);
}

TEST_F(DataReductionProxyUsageStatsTest, ProxyReachableThenUnreachable) {
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("foo.com", net::ProxyServer::SCHEME_HTTP);
  scoped_ptr<DataReductionProxyUsageStats> usage_stats(
      new DataReductionProxyUsageStats(
          &mock_params_, loop_proxy_));
  usage_stats->set_unavailable_callback(
      base::Bind(&DataReductionProxyUsageStatsTest::NotifyUnavailable,
                 base::Unretained(this)));
  EXPECT_CALL(mock_params_,
              WasDataReductionProxyUsed(mock_url_request_.get(), NULL))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_params_, IsDataReductionProxy(testing::_, testing::_))
      .WillRepeatedly(testing::Return(true));

  // Proxy succeeds.
  usage_stats->OnUrlRequestCompleted(mock_url_request_.get(), false);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(unavailable_);

  // Then proxy falls back indefinitely.
  usage_stats->OnProxyFallback(fallback_proxy_server,
                               net::ERR_PROXY_CONNECTION_FAILED);
  usage_stats->OnProxyFallback(fallback_proxy_server,
                                 net::ERR_PROXY_CONNECTION_FAILED);
  usage_stats->OnProxyFallback(fallback_proxy_server,
                                 net::ERR_PROXY_CONNECTION_FAILED);
  usage_stats->OnProxyFallback(fallback_proxy_server,
                                 net::ERR_PROXY_CONNECTION_FAILED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(unavailable_);
}

TEST_F(DataReductionProxyUsageStatsTest,
       DetectAndRecordMissingViaHeaderResponseCode) {
  const std::string kPrimaryHistogramName =
      "DataReductionProxy.MissingViaHeader.ResponseCode.Primary";
  const std::string kFallbackHistogramName =
      "DataReductionProxy.MissingViaHeader.ResponseCode.Fallback";

  struct TestCase {
    bool is_primary;
    const char* headers;
    int expected_primary_sample;   // -1 indicates no expected sample.
    int expected_fallback_sample;  // -1 indicates no expected sample.
  };
  const TestCase test_cases[] = {
    {
      true,
      "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      -1,
      -1
    },
    {
      false,
      "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      -1,
      -1
    },
    {
      true,
      "HTTP/1.1 200 OK\n",
      200,
      -1
    },
    {
      false,
      "HTTP/1.1 200 OK\n",
      -1,
      200
    },
    {
      true,
      "HTTP/1.1 304 Not Modified\n",
      304,
      -1
    },
    {
      false,
      "HTTP/1.1 304 Not Modified\n",
      -1,
      304
    },
    {
      true,
      "HTTP/1.1 404 Not Found\n",
      404,
      -1
    },
    {
      false,
      "HTTP/1.1 404 Not Found\n",
      -1,
      404
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    base::HistogramTester histogram_tester;
    std::string raw_headers(test_cases[i].headers);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    DataReductionProxyUsageStats::DetectAndRecordMissingViaHeaderResponseCode(
        test_cases[i].is_primary, headers.get());

    if (test_cases[i].expected_primary_sample == -1) {
      histogram_tester.ExpectTotalCount(kPrimaryHistogramName, 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kPrimaryHistogramName, test_cases[i].expected_primary_sample, 1);
    }

    if (test_cases[i].expected_fallback_sample == -1) {
      histogram_tester.ExpectTotalCount(kFallbackHistogramName, 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kFallbackHistogramName, test_cases[i].expected_fallback_sample, 1);
    }
  }
}

TEST_F(DataReductionProxyUsageStatsTest, RecordMissingViaHeaderBytes) {
  const std::string k4xxHistogramName =
      "DataReductionProxy.MissingViaHeader.Bytes.4xx";
  const std::string kOtherHistogramName =
      "DataReductionProxy.MissingViaHeader.Bytes.Other";
  const int64 kResponseContentLength = 100;

  struct TestCase {
    bool was_proxy_used;
    const char* headers;
    bool is_4xx_sample_expected;
    bool is_other_sample_expected;
  };
  const TestCase test_cases[] = {
    // Nothing should be recorded for requests that don't use the proxy.
    {
      false,
      "HTTP/1.1 404 Not Found\n",
      false,
      false
    },
    {
      false,
      "HTTP/1.1 200 OK\n",
      false,
      false
    },
    // Nothing should be recorded for responses that have the via header.
    {
      true,
      "HTTP/1.1 404 Not Found\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      false,
      false
    },
    {
      true,
      "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      false,
      false
    },
    // 4xx responses that used the proxy and don't have the via header should be
    // recorded.
    {
      true,
      "HTTP/1.1 404 Not Found\n",
      true,
      false
    },
    {
      true,
      "HTTP/1.1 400 Bad Request\n",
      true,
      false
    },
    {
      true,
      "HTTP/1.1 499 Big Client Error Response Code\n",
      true,
      false
    },
    // Non-4xx responses that used the proxy and don't have the via header
    // should be recorded.
    {
      true,
      "HTTP/1.1 200 OK\n",
      false,
      true
    },
    {
      true,
      "HTTP/1.1 399 Big Redirection Response Code\n",
      false,
      true
    },
    {
      true,
      "HTTP/1.1 500 Internal Server Error\n",
      false,
      true
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    base::HistogramTester histogram_tester;
    scoped_ptr<DataReductionProxyUsageStats> usage_stats(
        new DataReductionProxyUsageStats(&mock_params_, loop_proxy_));

    std::string raw_headers(test_cases[i].headers);
    HeadersToRaw(&raw_headers);

    scoped_ptr<net::URLRequest> fake_request(
        CreateURLRequestWithResponseHeaders(GURL("http://www.google.com/"),
                                            raw_headers));
    fake_request->set_received_response_content_length(kResponseContentLength);

    EXPECT_CALL(mock_params_,
                WasDataReductionProxyUsed(fake_request.get(), NULL))
        .WillRepeatedly(Return(test_cases[i].was_proxy_used));

    usage_stats->RecordMissingViaHeaderBytes(fake_request.get());

    if (test_cases[i].is_4xx_sample_expected) {
      histogram_tester.ExpectUniqueSample(k4xxHistogramName,
                                          kResponseContentLength, 1);
    } else {
      histogram_tester.ExpectTotalCount(k4xxHistogramName, 0);
    }

    if (test_cases[i].is_other_sample_expected) {
      histogram_tester.ExpectUniqueSample(kOtherHistogramName,
                                          kResponseContentLength, 1);
    } else {
      histogram_tester.ExpectTotalCount(kOtherHistogramName, 0);
    }
  }
}

}  // namespace data_reduction_proxy

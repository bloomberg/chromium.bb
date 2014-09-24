// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
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
    mock_url_request_ = context_.CreateRequest(GURL(), net::IDLE, &delegate_,
                                               NULL);
  }

  void NotifyUnavailable(bool unavailable) {
    unavailable_ = unavailable;
  }

  // Required for base::MessageLoopProxy::current().
  base::MessageLoopForUI loop_;
  base::MessageLoopProxy* loop_proxy_;

 protected:
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  DataReductionProxyParamsMock mock_params_;
  scoped_ptr<net::URLRequest> mock_url_request_;
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

}  // namespace data_reduction_proxy

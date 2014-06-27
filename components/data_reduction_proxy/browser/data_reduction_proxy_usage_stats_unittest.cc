// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MessageLoop;
using base::MessageLoopProxy;
using data_reduction_proxy::DataReductionProxyParams;
using net::TestDelegate;
using net::TestURLRequestContext;
using net::URLRequest;
using net::URLRequestStatus;
using testing::Return;

namespace {

class DataReductionProxyParamsMock : public DataReductionProxyParams {
 public:
  DataReductionProxyParamsMock() : DataReductionProxyParams(0) {}
  virtual ~DataReductionProxyParamsMock() {}

  MOCK_METHOD1(IsDataReductionProxyEligible, bool(const net::URLRequest*));
  MOCK_CONST_METHOD2(WasDataReductionProxyUsed, bool(const net::URLRequest*,
      std::pair<GURL, GURL>* proxy_servers));

 private:
  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyParamsMock);
};

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyUsageStatsTest : public testing::Test {
 public:
  DataReductionProxyUsageStatsTest()
      : loop_proxy_(MessageLoopProxy::current().get()),
        context_(true),
        mock_url_request_(GURL(), net::IDLE, &delegate_, &context_) {
    context_.Init();
  }
  // Required for MessageLoopProxy::current().
  base::MessageLoopForUI loop_;
  MessageLoopProxy* loop_proxy_;

 protected:
  TestURLRequestContext context_;
  TestDelegate delegate_;
  DataReductionProxyParamsMock mock_params_;
  URLRequest mock_url_request_;
};

TEST_F(DataReductionProxyUsageStatsTest, isDataReductionProxyUnreachable) {
  struct TestCase {
    bool is_proxy_eligible;
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

    EXPECT_CALL(mock_params_, IsDataReductionProxyEligible(&mock_url_request_))
        .WillRepeatedly(Return(test_case.is_proxy_eligible));
    EXPECT_CALL(mock_params_,
                WasDataReductionProxyUsed(&mock_url_request_, NULL))
        .WillRepeatedly(Return(test_case.was_proxy_used));

    scoped_ptr<DataReductionProxyUsageStats> usage_stats(
        new DataReductionProxyUsageStats(
            &mock_params_, loop_proxy_, loop_proxy_));

    usage_stats->OnUrlRequestCompleted(&mock_url_request_, false);
    MessageLoop::current()->RunUntilIdle();

    EXPECT_EQ(test_case.is_unreachable,
              usage_stats->isDataReductionProxyUnreachable());
  }
}

}  // namespace data_reduction_proxy

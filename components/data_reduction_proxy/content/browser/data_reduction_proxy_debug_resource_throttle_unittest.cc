// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_resource_throttle.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/content/browser/content_data_reduction_proxy_debug_ui_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "content/public/common/resource_type.h"
#include "net/base/load_flags.h"
#include "net/proxy/proxy_config.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}

namespace data_reduction_proxy {

namespace {
class TestDataReductionProxyDebugResourceThrottle
    : public DataReductionProxyDebugResourceThrottle {
 public:
  TestDataReductionProxyDebugResourceThrottle(
      net::URLRequest* request,
      content::ResourceType resource_type,
      DataReductionProxyDebugUIService* ui_service,
      const DataReductionProxyConfig* config)
      : DataReductionProxyDebugResourceThrottle(
            request, resource_type, ui_service, config) {
  }

  ~TestDataReductionProxyDebugResourceThrottle() override {
  }

 private:
  void DisplayBlockingPage(bool* defer) override {
    *defer = true;
  }

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyDebugResourceThrottle);
};
}  // namespace

class DataReductionProxyDebugResourceThrottleTest : public testing::Test {
 public:
  DataReductionProxyDebugResourceThrottleTest() : context_(true) {
    context_.Init();

    // The |test_job_factory_| takes ownership of the interceptor.
    test_job_interceptor_ = new net::TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(url::kHttpScheme,
                                                     test_job_interceptor_));

    context_.set_job_factory(&test_job_factory_);

    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed)
            .WithParamsDefinitions(TestDataReductionProxyParams::HAS_EVERYTHING)
            .WithMockConfig()
            .Build();
    request_ = context_.CreateRequest(GURL("http://www.google.com/"), net::IDLE,
                                      &delegate_, NULL);
    ui_service_.reset(new ContentDataReductionProxyDebugUIService(
        base::Bind(
            &DataReductionProxyDebugResourceThrottleTest::
                data_reduction_proxy_config,
            base::Unretained(this)),
        test_context_->task_runner(),
        test_context_->task_runner(),
        "en-US"));
    resource_throttle_.reset(new TestDataReductionProxyDebugResourceThrottle(
        request_.get(), content::RESOURCE_TYPE_MAIN_FRAME,
        ui_service_.get(), test_context_->config()));
  }

  const net::ProxyConfig& data_reduction_proxy_config() const {
    return proxy_config_;
  }

 protected:
  MockDataReductionProxyConfig* config() const {
    return test_context_->mock_config();
  }

  TestDataReductionProxyDebugResourceThrottle* throttle() const {
    return resource_throttle_.get();
  }

  net::URLRequest* request() const {
    return request_.get();
  }

 private:
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  // |test_job_interceptor_| is owned by |test_job_factory_|.
  net::TestJobInterceptor* test_job_interceptor_;
  net::URLRequestJobFactoryImpl test_job_factory_;

  net::ProxyConfig proxy_config_;
  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<ContentDataReductionProxyDebugUIService> ui_service_;
  scoped_ptr<TestDataReductionProxyDebugResourceThrottle> resource_throttle_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
};

// Expect to show the interstitial (defer == true), when the data reduction
// proxy is bypassed by the proxy server, but not when bypassed by local rules
// on the client (even when the proxy has already been bypassed by the server).
TEST_F(DataReductionProxyDebugResourceThrottleTest, WillStartUsingNetwork) {
  bool defer = false;
  EXPECT_CALL(
      *config(), AreDataReductionProxiesBypassed(
          testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Return(false));
  throttle()->WillStartUsingNetwork(&defer);
  EXPECT_FALSE(defer);

  EXPECT_CALL(
      *config(), AreDataReductionProxiesBypassed(
          testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *config(), IsBypassedByDataReductionProxyLocalRules(
          testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Return(false));
  throttle()->WillStartUsingNetwork(&defer);
  EXPECT_TRUE(defer);

  // Must be tested last because this sets |state_| of |resource_throttle_|
  // to LOCAL_BYPASS.
  defer = false;
  EXPECT_CALL(
      *config(), AreDataReductionProxiesBypassed(
          testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *config(), IsBypassedByDataReductionProxyLocalRules(
          testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  throttle()->WillStartUsingNetwork(&defer);
  EXPECT_FALSE(defer);
}

// Expect to show the interstitial (defer = true) when the LOAD_BYPASS_PROXY
// flag is not set, but not when it is.
TEST_F(DataReductionProxyDebugResourceThrottleTest, WillRedirectRequest) {
  bool defer = false;
  throttle()->WillRedirectRequest(net::RedirectInfo(), &defer);
  EXPECT_FALSE(defer);

  request()->SetLoadFlags(net::LOAD_BYPASS_PROXY);
  throttle()->WillRedirectRequest(net::RedirectInfo(), &defer);
  EXPECT_TRUE(defer);
}

}  // namespace data_reduction_proxy

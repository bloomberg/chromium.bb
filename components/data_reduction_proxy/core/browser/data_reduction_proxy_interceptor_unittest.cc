// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/capturing_net_log.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class CountingURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  CountingURLRequestInterceptor()
      : request_count_(0), redirect_count_(0), response_count_(0) {
  }

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    request_count_++;
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override {
    redirect_count_++;
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    response_count_++;
    return nullptr;
  }

  int request_count() const {
    return request_count_;
  }

  int redirect_count() const {
    return redirect_count_;
  }

  int response_count() const {
    return response_count_;
  }

 private:
  mutable int request_count_;
  mutable int redirect_count_;
  mutable int response_count_;
};

class TestURLRequestContextWithDataReductionProxy
    : public net::TestURLRequestContext {
 public:
  TestURLRequestContextWithDataReductionProxy(DataReductionProxyParams* params,
                                              net::NetworkDelegate* delegate)
      : net::TestURLRequestContext(true) {
    std::string proxy = params->origin().spec();
    context_storage_.set_proxy_service(net::ProxyService::CreateFixed(proxy));
    set_network_delegate(delegate);
  }

  ~TestURLRequestContextWithDataReductionProxy() override {}
};

class DataReductionProxyInterceptorTest : public testing::Test {
 public:
  DataReductionProxyInterceptorTest()
      : params_(DataReductionProxyParams::kAllowed) {
    default_context_.reset(
        new TestURLRequestContextWithDataReductionProxy(
            &params_, &default_network_delegate_));
    default_context_->set_network_delegate(&default_network_delegate_);
    default_context_->set_net_log(&net_log_);
  }

  ~DataReductionProxyInterceptorTest() override {
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();
  }

  void Init(scoped_ptr<net::URLRequestJobFactory> factory) {
    job_factory_ = factory.Pass();
    default_context_->set_job_factory(job_factory_.get());
    default_context_->Init();
  }

  DataReductionProxyParams params_;
  net::CapturingNetLog net_log_;
  net::TestNetworkDelegate default_network_delegate_;
  scoped_ptr<net::URLRequestJobFactory> job_factory_;
  scoped_ptr<net::TestURLRequestContext> default_context_;
  base::MessageLoopForIO loop_;
};

TEST_F(DataReductionProxyInterceptorTest, TestJobFactoryChaining) {
  // Verifies that job factories can be chained.
  scoped_ptr<net::URLRequestJobFactory> impl(
      new net::URLRequestJobFactoryImpl());

  CountingURLRequestInterceptor* interceptor2 =
      new CountingURLRequestInterceptor();
  scoped_ptr<net::URLRequestJobFactory> factory2(
      new net::URLRequestInterceptingJobFactory(
          impl.Pass(), make_scoped_ptr(interceptor2)));

  CountingURLRequestInterceptor* interceptor1 =
      new CountingURLRequestInterceptor();
  scoped_ptr<net::URLRequestJobFactory> factory1(
      new net::URLRequestInterceptingJobFactory(
          factory2.Pass(), make_scoped_ptr(interceptor1)));

  Init(factory1.Pass());

  net::TestDelegate d;
  scoped_ptr<net::URLRequest> req(default_context_->CreateRequest(
      GURL("http://foo"), net::DEFAULT_PRIORITY, &d, nullptr));

  req->Start();
  base::RunLoop().Run();
  EXPECT_EQ(1, interceptor1->request_count());
  EXPECT_EQ(0, interceptor1->redirect_count());
  EXPECT_EQ(1, interceptor1->response_count());
  EXPECT_EQ(1, interceptor2->request_count());
  EXPECT_EQ(0, interceptor2->redirect_count());
  EXPECT_EQ(1, interceptor2->response_count());
}
}  // namespace data_reduction_proxy

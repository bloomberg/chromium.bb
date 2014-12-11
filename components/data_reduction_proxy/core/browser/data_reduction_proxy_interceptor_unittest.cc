// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/capturing_net_log.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

class DataReductionProxyInterceptorWithServerTest : public testing::Test {
 public:
  DataReductionProxyInterceptorWithServerTest()
      : context_(true) {
    context_.set_network_delegate(&network_delegate_);
    context_.set_net_log(&net_log_);
  }

  ~DataReductionProxyInterceptorWithServerTest() override {
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    base::FilePath root_path, proxy_file_path, direct_file_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &root_path);
    proxy_file_path = root_path.AppendASCII(
        "components/test/data/data_reduction_proxy/proxy");
    direct_file_path = root_path.AppendASCII(
        "components/test/data/data_reduction_proxy/direct");
    proxy_.ServeFilesFromDirectory(proxy_file_path);
    direct_.ServeFilesFromDirectory(direct_file_path);
    ASSERT_TRUE(proxy_.InitializeAndWaitUntilReady());
    ASSERT_TRUE(direct_.InitializeAndWaitUntilReady());

    params_.reset(
        new TestDataReductionProxyParams(
            DataReductionProxyParams::kAllowed,
            TestDataReductionProxyParams::HAS_EVERYTHING &
            ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
            ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));
    params_->set_origin(proxy_.GetURL("/"));
    std::string proxy_name =
        net::HostPortPair::FromURL(GURL(params_->origin())).ToString();
    proxy_service_.reset(
        net::ProxyService::CreateFixedFromPacResult(
            "PROXY " + proxy_name + "; DIRECT"));

    context_.set_proxy_service(proxy_service_.get());

    event_store_.reset(
        new DataReductionProxyEventStore(loop_.message_loop_proxy()));

    DataReductionProxyInterceptor* interceptor =
        new DataReductionProxyInterceptor(params_.get(), NULL,
                                          event_store_.get());

    scoped_ptr<net::URLRequestJobFactoryImpl> job_factory_impl(
        new net::URLRequestJobFactoryImpl());
    job_factory_.reset(
        new net::URLRequestInterceptingJobFactory(
            job_factory_impl.Pass(),
            make_scoped_ptr(interceptor)));
    context_.set_job_factory(job_factory_.get());
    context_.Init();
  }


  const net::TestURLRequestContext& context() {
    return context_;
  }

  const net::test_server::EmbeddedTestServer& direct() {
    return direct_;
  }

 private:
  net::CapturingNetLog net_log_;
  net::TestNetworkDelegate network_delegate_;

  net::TestURLRequestContext context_;
  net::test_server::EmbeddedTestServer proxy_;
  net::test_server::EmbeddedTestServer direct_;
  scoped_ptr<TestDataReductionProxyParams> params_;
  scoped_ptr<net::ProxyService> proxy_service_;
  scoped_ptr<DataReductionProxyEventStore> event_store_;
  scoped_ptr<net::URLRequestJobFactory> job_factory_;

  base::MessageLoopForIO loop_;
};

TEST_F(DataReductionProxyInterceptorWithServerTest, TestBypass) {
  // Tests the mechanics of proxy bypass work with a "real" server. For tests
  // that cover every imaginable response that could trigger a bypass, see:
  // DataReductionProxyProtocolTest.
  net::TestDelegate delegate;
  scoped_ptr<net::URLRequest> request(
      context().CreateRequest(direct().GetURL("/block10.html"),
                             net::DEFAULT_PRIORITY, &delegate, NULL));
  request->Start();
  EXPECT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request->status().status());
  EXPECT_EQ(net::OK, request->status().error());
  EXPECT_EQ("hello", delegate.data_received());
}

TEST_F(DataReductionProxyInterceptorWithServerTest, TestNoBypass) {
  net::TestDelegate delegate;
  scoped_ptr<net::URLRequest> request(
      context().CreateRequest(direct().GetURL("/noblock.html"),
                             net::DEFAULT_PRIORITY, &delegate, NULL));
  request->Start();
  EXPECT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request->status().status());
  EXPECT_EQ(net::OK, request->status().error());
  EXPECT_EQ("hello", delegate.data_received());
}

}  // namespace data_reduction_proxy

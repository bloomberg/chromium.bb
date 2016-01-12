// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/url_loader_impl.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

class TestURLRequestJob;

TestURLRequestJob* g_current_job = nullptr;

template <class A>
void PassA(A* destination, A value) {
  *destination = std::move(value);
}

class TestURLRequestJob : public net::URLRequestJob {
 public:
  enum Status { CREATED, STARTED, READING, COMPLETED };

  TestURLRequestJob(net::URLRequest* request,
                    net::NetworkDelegate* network_delegate)
      : net::URLRequestJob(request, network_delegate),
        status_(CREATED),
        buf_size_(0) {
    CHECK(!g_current_job);
    g_current_job = this;
  }

  Status status() { return status_; }

  int buf_size() { return buf_size_; }

  void Start() override { status_ = STARTED; }

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    status_ = READING;
    buf_size_ = buf_size;
    return net::ERR_IO_PENDING;
  }

  void NotifyHeadersComplete() { net::URLRequestJob::NotifyHeadersComplete(); }

  void NotifyReadComplete(int bytes_read) {
    if (bytes_read < 0) {
      // Map errors to net::ERR_FAILED.
      ReadRawDataComplete(net::ERR_FAILED);
      // Set this after calling ReadRawDataComplete since that ends up calling
      // ReadRawData.
      status_ = COMPLETED;
    } else if (bytes_read == 0) {
      ReadRawDataComplete(bytes_read);
      // Set this after calling ReadRawDataComplete since that ends up calling
      // ReadRawData.
      status_ = COMPLETED;
    } else {
      ReadRawDataComplete(bytes_read);
      // Set this after calling ReadRawDataComplete since that ends up calling
      // ReadRawData.
      status_ = STARTED;
    }
  }

 private:
  ~TestURLRequestJob() override {
    CHECK(g_current_job == this);
    g_current_job = nullptr;
  }

  Status status_;
  int buf_size_;
};

class TestProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit TestProtocolHandler(const base::Closure& quit_closure)
      : quit_closure_(quit_closure) {}
    net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    quit_closure_.Run();
    return new TestURLRequestJob(request, network_delegate);
  }

  ~TestProtocolHandler() override {}

 private:
  base::Closure quit_closure_;
};

class UrlLoaderImplTest : public test::ApplicationTestBase {
 public:
  UrlLoaderImplTest() : message_loop_(common::MessagePumpMojo::Create()) {}

 protected:
  bool ShouldCreateDefaultRunLoop() override {
    return false;
  }

  void SetUp() override {
    ApplicationTestBase::SetUp();

    scoped_ptr<net::TestURLRequestContext> url_request_context(
        new net::TestURLRequestContext(true));
    ASSERT_TRUE(url_request_job_factory_.SetProtocolHandler(
        "http", make_scoped_ptr(new TestProtocolHandler(
            wait_for_request_.QuitClosure()))));
    url_request_context->set_job_factory(&url_request_job_factory_);
    url_request_context->Init();
    network_context_.reset(new NetworkContext(std::move(url_request_context)));
    MessagePipe pipe;
    new URLLoaderImpl(network_context_.get(),
                      GetProxy(&url_loader_proxy_),
                      make_scoped_ptr<mojo::AppRefCount>(nullptr));
    EXPECT_TRUE(IsUrlLoaderValid());
  }

  bool IsUrlLoaderValid() {
    return network_context_->GetURLLoaderCountForTesting() > 0u;
  }

  base::MessageLoop message_loop_;
  net::TestJobInterceptor* job_interceptor_;
  net::URLRequestJobFactoryImpl url_request_job_factory_;
  scoped_ptr<NetworkContext> network_context_;
  URLLoaderPtr url_loader_proxy_;
  base::RunLoop wait_for_request_;
};

TEST_F(UrlLoaderImplTest, ClosedBeforeAnyCall) {
  url_loader_proxy_.reset();

  while (IsUrlLoaderValid())
    base::RunLoop().RunUntilIdle();
}

TEST_F(UrlLoaderImplTest, ClosedWhileWaitingOnTheNetwork) {
  URLRequestPtr request(URLRequest::New());
  request->url = "http://example.com";

  URLResponsePtr response;
  url_loader_proxy_->Start(std::move(request),
                           base::Bind(&PassA<URLResponsePtr>, &response));
  wait_for_request_.Run();

  EXPECT_TRUE(IsUrlLoaderValid());
  EXPECT_FALSE(response);
  ASSERT_TRUE(g_current_job);

  g_current_job->NotifyHeadersComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsUrlLoaderValid());
  EXPECT_TRUE(response);
  EXPECT_EQ(TestURLRequestJob::READING, g_current_job->status());

  url_loader_proxy_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsUrlLoaderValid());

  response.reset();

  while (IsUrlLoaderValid())
    base::RunLoop().RunUntilIdle();
}

TEST_F(UrlLoaderImplTest, ClosedWhileWaitingOnThePipeToBeWriteable) {
  URLRequestPtr request(URLRequest::New());
  request->url = "http://example.com";

  URLResponsePtr response;
  url_loader_proxy_->Start(std::move(request),
                           base::Bind(&PassA<URLResponsePtr>, &response));
  wait_for_request_.Run();

  EXPECT_TRUE(IsUrlLoaderValid());
  EXPECT_FALSE(response);
  ASSERT_TRUE(g_current_job);

  g_current_job->NotifyHeadersComplete();
  while (g_current_job->status() != TestURLRequestJob::READING)
    base::RunLoop().RunUntilIdle();

  while (!response)
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsUrlLoaderValid());
  EXPECT_EQ(TestURLRequestJob::READING, g_current_job->status());

  while (g_current_job->status() != TestURLRequestJob::STARTED) {
    g_current_job->NotifyReadComplete(g_current_job->buf_size());
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(TestURLRequestJob::STARTED, g_current_job->status());

  url_loader_proxy_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsUrlLoaderValid());

  response.reset();

  while (IsUrlLoaderValid())
    base::RunLoop().RunUntilIdle();
}

TEST_F(UrlLoaderImplTest, RequestCompleted) {
  URLRequestPtr request(URLRequest::New());
  request->url = "http://example.com";

  URLResponsePtr response;
  url_loader_proxy_->Start(std::move(request),
                           base::Bind(&PassA<URLResponsePtr>, &response));
  wait_for_request_.Run();

  EXPECT_TRUE(IsUrlLoaderValid());
  EXPECT_FALSE(response);
  ASSERT_TRUE(g_current_job);

  g_current_job->NotifyHeadersComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsUrlLoaderValid());
  EXPECT_TRUE(response);
  EXPECT_EQ(TestURLRequestJob::READING, g_current_job->status());

  url_loader_proxy_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsUrlLoaderValid());

  g_current_job->NotifyReadComplete(0);

  while (IsUrlLoaderValid())
    base::RunLoop().RunUntilIdle();
}

TEST_F(UrlLoaderImplTest, RequestFailed) {
  URLRequestPtr request(URLRequest::New());
  request->url = "http://example.com";

  URLResponsePtr response;
  url_loader_proxy_->Start(std::move(request),
                           base::Bind(&PassA<URLResponsePtr>, &response));
  wait_for_request_.Run();

  EXPECT_TRUE(IsUrlLoaderValid());
  EXPECT_FALSE(response);
  ASSERT_TRUE(g_current_job);

  g_current_job->NotifyHeadersComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsUrlLoaderValid());
  EXPECT_TRUE(response);
  EXPECT_EQ(TestURLRequestJob::READING, g_current_job->status());

  url_loader_proxy_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsUrlLoaderValid());

  g_current_job->NotifyReadComplete(-1);

  while (IsUrlLoaderValid())
    base::RunLoop().RunUntilIdle();
}

}  // namespace mojo

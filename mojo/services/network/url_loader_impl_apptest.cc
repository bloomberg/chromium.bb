// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/common/message_pump_mojo.h"
#include "mojo/public/cpp/application/application_test_base.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/url_loader_impl.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class TestURLRequestJob;

TestURLRequestJob* g_current_job = nullptr;

template <class A>
void PassA(A* destination, A value) {
  *destination = value.Pass();
}

// Extends URLLoaderImpl to allow to get a weak pointer and check if it has been
// correctly deleted.
class TestedURLLoaderImpl : public URLLoaderImpl,
                            public base::SupportsWeakPtr<TestedURLLoaderImpl> {
 public:
  TestedURLLoaderImpl(NetworkContext* context,
                      InterfaceRequest<URLLoader> request)
      : URLLoaderImpl(context, request.Pass()) {}
};

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

  bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read) override {
    status_ = READING;
    buf_size_ = buf_size;
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
    return false;
  }

  void NotifyHeadersComplete() { net::URLRequestJob::NotifyHeadersComplete(); }

  void NotifyReadComplete(int bytes_read) {
    if (bytes_read < 0) {
      status_ = COMPLETED;
      NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, 0));
      net::URLRequestJob::NotifyReadComplete(0);
    } else if (bytes_read == 0) {
      status_ = COMPLETED;
      NotifyDone(net::URLRequestStatus());
      net::URLRequestJob::NotifyReadComplete(bytes_read);
    } else {
      status_ = STARTED;
      SetStatus(net::URLRequestStatus());
      net::URLRequestJob::NotifyReadComplete(bytes_read);
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
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new TestURLRequestJob(request, network_delegate);
  }

 private:
  ~TestProtocolHandler() override {}
};

class UrlLoaderImplTest : public test::ApplicationTestBase {
 public:
  UrlLoaderImplTest() : message_loop_(common::MessagePumpMojo::Create()) {}

 protected:
  bool ShouldCreateDefaultRunLoop() override {
    return false;
  }

  void SetUp() {
    ApplicationTestBase::SetUp();

    scoped_ptr<net::TestURLRequestContext> url_request_context(
        new net::TestURLRequestContext(true));
    ASSERT_TRUE(url_request_job_factory_.SetProtocolHandler(
        "http", new TestProtocolHandler()));
    url_request_context->set_job_factory(&url_request_job_factory_);
    url_request_context->Init();
    network_context_.reset(new NetworkContext(url_request_context.Pass()));
    MessagePipe pipe;
    url_loader_proxy_ = MakeProxy<URLLoader>(pipe.handle0.Pass());
    url_loader_ =
        (new TestedURLLoaderImpl(network_context_.get(),
                                 MakeRequest<URLLoader>(pipe.handle1.Pass())))
            ->AsWeakPtr();
    EXPECT_TRUE(url_loader_);
  }

  base::MessageLoop message_loop_;
  net::TestJobInterceptor* job_interceptor_;
  net::URLRequestJobFactoryImpl url_request_job_factory_;
  scoped_ptr<NetworkContext> network_context_;
  URLLoaderPtr url_loader_proxy_;
  base::WeakPtr<TestedURLLoaderImpl> url_loader_;
};

TEST_F(UrlLoaderImplTest, ClosedBeforeAnyCall) {
  url_loader_proxy_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(url_loader_);
}

TEST_F(UrlLoaderImplTest, ClosedWhileWaitingOnTheNetwork) {
  URLRequestPtr request(URLRequest::New());
  request->url = "http://example.com";

  URLResponsePtr response;
  url_loader_proxy_->Start(request.Pass(),
                           base::Bind(&PassA<URLResponsePtr>, &response));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);
  EXPECT_FALSE(response);
  ASSERT_TRUE(g_current_job);

  g_current_job->NotifyHeadersComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);
  EXPECT_TRUE(response);
  EXPECT_EQ(TestURLRequestJob::READING, g_current_job->status());

  url_loader_proxy_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);

  response.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(url_loader_);
}

TEST_F(UrlLoaderImplTest, ClosedWhileWaitingOnThePipeToBeWriteable) {
  URLRequestPtr request(URLRequest::New());
  request->url = "http://example.com";

  URLResponsePtr response;
  url_loader_proxy_->Start(request.Pass(),
                           base::Bind(&PassA<URLResponsePtr>, &response));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);
  EXPECT_FALSE(response);
  ASSERT_TRUE(g_current_job);

  g_current_job->NotifyHeadersComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);
  EXPECT_TRUE(response);
  EXPECT_EQ(TestURLRequestJob::READING, g_current_job->status());

  while (g_current_job->status() == TestURLRequestJob::READING) {
    g_current_job->NotifyReadComplete(g_current_job->buf_size());
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(TestURLRequestJob::STARTED, g_current_job->status());

  url_loader_proxy_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);

  response.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(url_loader_);
}

TEST_F(UrlLoaderImplTest, RequestCompleted) {
  URLRequestPtr request(URLRequest::New());
  request->url = "http://example.com";

  URLResponsePtr response;
  url_loader_proxy_->Start(request.Pass(),
                           base::Bind(&PassA<URLResponsePtr>, &response));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);
  EXPECT_FALSE(response);
  ASSERT_TRUE(g_current_job);

  g_current_job->NotifyHeadersComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);
  EXPECT_TRUE(response);
  EXPECT_EQ(TestURLRequestJob::READING, g_current_job->status());

  url_loader_proxy_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);

  g_current_job->NotifyReadComplete(0);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(url_loader_);
}

TEST_F(UrlLoaderImplTest, RequestFailed) {
  URLRequestPtr request(URLRequest::New());
  request->url = "http://example.com";

  URLResponsePtr response;
  url_loader_proxy_->Start(request.Pass(),
                           base::Bind(&PassA<URLResponsePtr>, &response));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);
  EXPECT_FALSE(response);
  ASSERT_TRUE(g_current_job);

  g_current_job->NotifyHeadersComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);
  EXPECT_TRUE(response);
  EXPECT_EQ(TestURLRequestJob::READING, g_current_job->status());

  url_loader_proxy_.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(url_loader_);

  g_current_job->NotifyReadComplete(-1);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(url_loader_);
}

}  // namespace mojo
}  // namespace

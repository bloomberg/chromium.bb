// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kBufferSize = 1024;
const int kProcessID = 1;
const int kProviderID = 100;

class MockURLRequestDelegate : public net::URLRequest::Delegate {
 public:
  MockURLRequestDelegate() : received_data_(new net::IOBuffer(kBufferSize)) {}
  virtual ~MockURLRequestDelegate() {}
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {
    if (request->status().is_success()) {
      EXPECT_TRUE(request->response_headers());
      Read(request);
    }
  }
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE {
    EXPECT_EQ(0, bytes_read);
  }

 private:
  void Read(net::URLRequest* request) {
    if (request->is_pending()) {
      int bytes_read = 0;
      request->Read(received_data_.get(), kBufferSize, &bytes_read);
      // For now ServiceWorkerURLRequestJob wouldn't return
      // any content data yet.
      EXPECT_EQ(0, bytes_read);
    }
  }

  scoped_refptr<net::IOBuffer> received_data_;
  base::Closure on_complete_;
};

class MockProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  MockProtocolHandler(base::WeakPtr<ServiceWorkerProviderHost> provider_host)
      : provider_host_(provider_host) {}
  virtual ~MockProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    ServiceWorkerURLRequestJob* job = new ServiceWorkerURLRequestJob(
        request, network_delegate, provider_host_);
    job->ForwardToServiceWorker();
    return job;
  }

 private:
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
};

}  // namespace

class ServiceWorkerURLRequestJobTest : public testing::Test {
 protected:
  ServiceWorkerURLRequestJobTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~ServiceWorkerURLRequestJobTest() {}

  virtual void SetUp() OVERRIDE {
    context_.reset(new ServiceWorkerContextCore(base::FilePath(), NULL, NULL));
    helper_.reset(new EmbeddedWorkerTestHelper(context_.get(), kProcessID));

    registration_ = new ServiceWorkerRegistration(
        GURL("http://example.com/*"),
        GURL("http://example.com/service_worker.js"),
        1L, context_->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_,
        1L, context_->AsWeakPtr());

    scoped_ptr<ServiceWorkerProviderHost> provider_host(
        new ServiceWorkerProviderHost(kProcessID, kProviderID,
                                      context_->AsWeakPtr(), NULL));
    provider_host->SetActiveVersion(version_.get());

    url_request_job_factory_.SetProtocolHandler(
        "http", new MockProtocolHandler(provider_host->AsWeakPtr()));
    url_request_context_.set_job_factory(&url_request_job_factory_);

    context_->AddProviderHost(provider_host.Pass());
  }

  virtual void TearDown() OVERRIDE {
    version_ = NULL;
    registration_ = NULL;
    helper_.reset();
    context_.reset();
  }

  void TestRequest() {
    request_ = url_request_context_.CreateRequest(
        GURL("http://example.com/foo.html"),
        net::DEFAULT_PRIORITY,
        &url_request_delegate_,
        NULL);

    request_->set_method("GET");
    request_->Start();
    base::RunLoop().RunUntilIdle();

    // Verify response.
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_EQ(200, request_->response_headers()->response_code());
  }

  TestBrowserThreadBundle thread_bundle_;

  scoped_ptr<ServiceWorkerContextCore> context_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;

  net::URLRequestJobFactoryImpl url_request_job_factory_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  scoped_ptr<net::URLRequest> request_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLRequestJobTest);
};

TEST_F(ServiceWorkerURLRequestJobTest, Simple) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVE);
  TestRequest();
}

TEST_F(ServiceWorkerURLRequestJobTest, WaitForActivation) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  version_->DispatchActivateEvent(CreateReceiverOnCurrentThread(&status));
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());

  TestRequest();

  EXPECT_EQ(SERVICE_WORKER_OK, status);
}

// TODO(kinuko): Add more tests with different response data and also for
// FallbackToNetwork case.

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/resource_request_body.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

int kMockRenderProcessId = 1224;
int kMockProviderId = 1;
const char kHeaders[] =
    "HTTP/1.1 200 OK\0"
    "Content-Type: text/javascript\0"
    "Expires: Thu, 1 Jan 2100 20:00:00 GMT\0"
    "\0";
const char kScriptCode[] = "// no script code\n";

void EmptyCallback() {
}

net::URLRequestJob* CreateNormalURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new net::URLRequestTestJob(request,
                                    network_delegate,
                                    std::string(kHeaders, arraysize(kHeaders)),
                                    kScriptCode,
                                    true);
}

net::URLRequestJob* CreateInvalidMimeTypeJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  const char kPlainTextHeaders[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: text/plain\0"
      "Expires: Thu, 1 Jan 2100 20:00:00 GMT\0"
      "\0";
  return new net::URLRequestTestJob(
      request,
      network_delegate,
      std::string(kPlainTextHeaders, arraysize(kPlainTextHeaders)),
      kScriptCode,
      true);
}

class SSLCertificateErrorJob : public net::URLRequestTestJob {
 public:
  SSLCertificateErrorJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& response_headers,
                         const std::string& response_data,
                         bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance),
        weak_factory_(this) {}
  void Start() override {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SSLCertificateErrorJob::NotifyError,
                   weak_factory_.GetWeakPtr()));
  }
  void NotifyError() {
    net::SSLInfo info;
    info.cert_status = net::CERT_STATUS_DATE_INVALID;
    NotifySSLCertificateError(info, true);
  }

 protected:
  ~SSLCertificateErrorJob() override {}
  base::WeakPtrFactory<SSLCertificateErrorJob> weak_factory_;
};

net::URLRequestJob* CreateSSLCertificateErrorJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new SSLCertificateErrorJob(request,
                                    network_delegate,
                                    std::string(kHeaders, arraysize(kHeaders)),
                                    kScriptCode,
                                    true);
}

class CertStatusErrorJob : public net::URLRequestTestJob {
 public:
  CertStatusErrorJob(net::URLRequest* request,
                     net::NetworkDelegate* network_delegate,
                     const std::string& response_headers,
                     const std::string& response_data,
                     bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}
  void GetResponseInfo(net::HttpResponseInfo* info) override {
    URLRequestTestJob::GetResponseInfo(info);
    info->ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  }

 protected:
  ~CertStatusErrorJob() override {}
};

net::URLRequestJob* CreateCertStatusErrorJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new CertStatusErrorJob(request,
                                network_delegate,
                                std::string(kHeaders, arraysize(kHeaders)),
                                kScriptCode,
                                true);
}

class MockHttpProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  typedef base::Callback<
      net::URLRequestJob*(net::URLRequest*, net::NetworkDelegate*)> JobCallback;

  MockHttpProtocolHandler(ResourceContext* resource_context)
      : resource_context_(resource_context) {}
  ~MockHttpProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    ServiceWorkerRequestHandler* handler =
        ServiceWorkerRequestHandler::GetHandler(request);
    if (handler) {
      return handler->MaybeCreateJob(
          request, network_delegate, resource_context_);
    }
    return create_job_callback_.Run(request, network_delegate);
  }
  void SetCreateJobCallback(const JobCallback& callback) {
    create_job_callback_ = callback;
  }

 private:
  ResourceContext* resource_context_;
  JobCallback create_job_callback_;
};
}

class ServiceWorkerWriteToCacheJobTest : public testing::Test {
 public:
  ServiceWorkerWriteToCacheJobTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        mock_protocol_handler_(nullptr) {}
  ~ServiceWorkerWriteToCacheJobTest() override {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(kMockRenderProcessId));

    // A new unstored registration/version.
    scope_ = GURL("https://host/scope/");
    script_url_ = GURL("https://host/script.js");
    registration_ =
        new ServiceWorkerRegistration(scope_, 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), script_url_, 1L, context()->AsWeakPtr());

    // An empty host.
    scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
        kMockRenderProcessId,
        MSG_ROUTING_NONE,
        kMockProviderId,
        SERVICE_WORKER_PROVIDER_FOR_CONTROLLEE,
        context()->AsWeakPtr(),
        nullptr));
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(host.Pass());
    provider_host_->running_hosted_version_ = version_;

    context()->storage()->LazyInitialize(base::Bind(&EmptyCallback));
    base::RunLoop().RunUntilIdle();

    url_request_context_.reset(new net::URLRequestContext);
    mock_protocol_handler_ = new MockHttpProtocolHandler(&resource_context_);
    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler("https",
                                                 mock_protocol_handler_);
    url_request_context_->set_job_factory(url_request_job_factory_.get());

    request_ = url_request_context_->CreateRequest(
        script_url_, net::DEFAULT_PRIORITY, &url_request_delegate_, nullptr);
    ServiceWorkerRequestHandler::InitializeHandler(
        request_.get(),
        context_wrapper(),
        &blob_storage_context_,
        kMockRenderProcessId,
        kMockProviderId,
        false,
        FETCH_REQUEST_MODE_NO_CORS,
        FETCH_CREDENTIALS_MODE_OMIT,
        RESOURCE_TYPE_SERVICE_WORKER,
        REQUEST_CONTEXT_TYPE_SERVICE_WORKER,
        REQUEST_CONTEXT_FRAME_TYPE_NONE,
        scoped_refptr<ResourceRequestBody>());
  }

  void TearDown() override {
    request_.reset();
    url_request_context_.reset();
    url_request_job_factory_.reset();
    mock_protocol_handler_ = nullptr;
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() const {
    return helper_->context_wrapper();
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  scoped_ptr<net::URLRequest> request_;
  MockHttpProtocolHandler* mock_protocol_handler_;

  storage::BlobStorageContext blob_storage_context_;
  content::MockResourceContext resource_context_;

  MockURLRequestDelegate url_request_delegate_;
  GURL scope_;
  GURL script_url_;
};

TEST_F(ServiceWorkerWriteToCacheJobTest, Normal) {
  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateNormalURLRequestJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request_->status().status());
  EXPECT_NE(kInvalidServiceWorkerResponseId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, InvalidMimeType) {
  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateInvalidMimeTypeJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResponseId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, SSLCertificateError) {
  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateSSLCertificateErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResponseId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, CertStatusError) {
  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateCertStatusErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResponseId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

}  // namespace content

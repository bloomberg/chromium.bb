// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_url_request_job.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/base/test_data_directory.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerURLRequestJobTest;

namespace {

const int kProcessID = 1;
const int kProviderID = 100;
const char kTestData[] = "Here is sample text for the blob.";

class MockHttpProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  MockHttpProtocolHandler(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      const ResourceContext* resource_context,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context)
      : provider_host_(provider_host),
        resource_context_(resource_context),
        blob_storage_context_(blob_storage_context) {}
  ~MockHttpProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    ServiceWorkerURLRequestJob* job =
        new ServiceWorkerURLRequestJob(request,
                                       network_delegate,
                                       provider_host_,
                                       blob_storage_context_,
                                       resource_context_,
                                       FETCH_REQUEST_MODE_NO_CORS,
                                       FETCH_CREDENTIALS_MODE_OMIT,
                                       REQUEST_CONTEXT_TYPE_HYPERLINK,
                                       REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL,
                                       scoped_refptr<ResourceRequestBody>());
    job->ForwardToServiceWorker();
    return job;
  }

 private:
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  const ResourceContext* resource_context_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
};

// Returns a BlobProtocolHandler that uses |blob_storage_context|. Caller owns
// the memory.
storage::BlobProtocolHandler* CreateMockBlobProtocolHandler(
    storage::BlobStorageContext* blob_storage_context) {
  // The FileSystemContext and MessageLoopProxy are not actually used but a
  // MessageLoopProxy is needed to avoid a DCHECK in BlobURLRequestJob ctor.
  return new storage::BlobProtocolHandler(
      blob_storage_context, nullptr, base::MessageLoopProxy::current().get());
}

}  // namespace

class ServiceWorkerURLRequestJobTest : public testing::Test {
 protected:
  ServiceWorkerURLRequestJobTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        blob_data_(new storage::BlobDataBuilder("blob-id:myblob")) {}
  ~ServiceWorkerURLRequestJobTest() override {}

  void SetUp() override {
    browser_context_.reset(new TestBrowserContext);
    InitializeResourceContext(browser_context_.get());
    SetUpWithHelper(new EmbeddedWorkerTestHelper(kProcessID));
  }

  void SetUpWithHelper(EmbeddedWorkerTestHelper* helper) {
    helper_.reset(helper);

    registration_ = new ServiceWorkerRegistration(
        GURL("http://example.com/"),
        1L,
        helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(),
        GURL("http://example.com/service_worker.js"),
        1L,
        helper_->context()->AsWeakPtr());

    // Make the registration findable via storage functions.
    helper_->context()->storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    helper_->context()->storage()->StoreRegistration(
        registration_.get(),
        version_.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(SERVICE_WORKER_OK, status);

    net::HttpResponseInfo http_info;
    http_info.ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                "ok_cert.pem");
    EXPECT_TRUE(http_info.ssl_info.is_valid());
    http_info.ssl_info.security_bits = 0x100;
    // SSL3 TLS_DHE_RSA_WITH_AES_256_CBC_SHA
    http_info.ssl_info.connection_status = 0x300039;
    version_->SetMainScriptHttpResponseInfo(http_info);

    scoped_ptr<ServiceWorkerProviderHost> provider_host(
        new ServiceWorkerProviderHost(
            kProcessID,
            MSG_ROUTING_NONE,
            kProviderID,
            SERVICE_WORKER_PROVIDER_FOR_CONTROLLEE,
            helper_->context()->AsWeakPtr(),
            nullptr));
    provider_host->SetDocumentUrl(GURL("http://example.com/"));
    provider_host->AssociateRegistration(registration_.get());
    registration_->SetActiveVersion(version_.get());

    ChromeBlobStorageContext* chrome_blob_storage_context =
        ChromeBlobStorageContext::GetFor(browser_context_.get());
    // Wait for chrome_blob_storage_context to finish initializing.
    base::RunLoop().RunUntilIdle();
    storage::BlobStorageContext* blob_storage_context =
        chrome_blob_storage_context->context();

    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler(
        "http",
        new MockHttpProtocolHandler(provider_host->AsWeakPtr(),
                                    browser_context_->GetResourceContext(),
                                    blob_storage_context->AsWeakPtr()));
    url_request_job_factory_->SetProtocolHandler(
        "blob", CreateMockBlobProtocolHandler(blob_storage_context));
    url_request_context_.set_job_factory(url_request_job_factory_.get());

    helper_->context()->AddProviderHost(provider_host.Pass());
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  void TestRequest(int expected_status_code,
                   const std::string& expected_status_text,
                   const std::string& expected_response) {
    request_ = url_request_context_.CreateRequest(
        GURL("http://example.com/foo.html"),
        net::DEFAULT_PRIORITY,
        &url_request_delegate_,
        nullptr);

    request_->set_method("GET");
    request_->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_EQ(expected_status_code,
              request_->response_headers()->response_code());
    EXPECT_EQ(expected_status_text,
              request_->response_headers()->GetStatusText());
    EXPECT_EQ(expected_response, url_request_delegate_.response_data());
    const net::SSLInfo& ssl_info = request_->response_info().ssl_info;
    EXPECT_TRUE(ssl_info.is_valid());
    EXPECT_EQ(ssl_info.security_bits, 0x100);
    EXPECT_EQ(ssl_info.connection_status, 0x300039);
  }

  bool HasInflightRequests() {
    return version_->HasInflightRequests();
  }

  TestBrowserThreadBundle thread_bundle_;

  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;

  scoped_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  scoped_ptr<net::URLRequest> request_;

  scoped_ptr<storage::BlobDataBuilder> blob_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerURLRequestJobTest);
};

TEST_F(ServiceWorkerURLRequestJobTest, Simple) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(200, "OK", std::string());
}

// Responds to fetch events with a blob.
class BlobResponder : public EmbeddedWorkerTestHelper {
 public:
  BlobResponder(int mock_render_process_id,
                const std::string& blob_uuid,
                uint64 blob_size)
      : EmbeddedWorkerTestHelper(mock_render_process_id),
        blob_uuid_(blob_uuid),
        blob_size_(blob_size) {}
  ~BlobResponder() override {}

 protected:
  void OnFetchEvent(int embedded_worker_id,
                    int request_id,
                    const ServiceWorkerFetchRequest& request) override {
    SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
        embedded_worker_id,
        request_id,
        SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
        ServiceWorkerResponse(GURL(),
                              200,
                              "OK",
                              blink::WebServiceWorkerResponseTypeDefault,
                              ServiceWorkerHeaderMap(),
                              blob_uuid_,
                              blob_size_,
                              GURL())));
  }

  std::string blob_uuid_;
  uint64 blob_size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlobResponder);
};

TEST_F(ServiceWorkerURLRequestJobTest, BlobResponse) {
  ChromeBlobStorageContext* blob_storage_context =
      ChromeBlobStorageContext::GetFor(browser_context_.get());
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    blob_data_->AppendData(kTestData);
    expected_response += kTestData;
  }
  scoped_ptr<storage::BlobDataHandle> blob_handle =
      blob_storage_context->context()->AddFinishedBlob(blob_data_.get());
  SetUpWithHelper(new BlobResponder(
      kProcessID, blob_handle->uuid(), expected_response.size()));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(200, "OK", expected_response);
}

TEST_F(ServiceWorkerURLRequestJobTest, NonExistentBlobUUIDResponse) {
  SetUpWithHelper(new BlobResponder(kProcessID, "blob-id:nothing-is-here", 0));
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  TestRequest(500, "Service Worker Response Error", std::string());
}

// Responds to fetch events with a stream.
class StreamResponder : public EmbeddedWorkerTestHelper {
 public:
  StreamResponder(int mock_render_process_id,
                  const GURL& stream_url)
      : EmbeddedWorkerTestHelper(mock_render_process_id),
        stream_url_(stream_url) {}
  ~StreamResponder() override {}

 protected:
  void OnFetchEvent(int embedded_worker_id,
                    int request_id,
                    const ServiceWorkerFetchRequest& request) override {
    SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
        embedded_worker_id,
        request_id,
        SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
        ServiceWorkerResponse(GURL(),
                              200,
                              "OK",
                              blink::WebServiceWorkerResponseTypeDefault,
                              ServiceWorkerHeaderMap(),
                              "",
                              0,
                              stream_url_)));
  }

  const GURL stream_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamResponder);
};

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  SetUpWithHelper(new StreamResponder(kProcessID, stream_url));
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("http://example.com/foo.html"),
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      nullptr);
  request_->set_method("GET");
  request_->Start();

  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  stream->Finalize();

  EXPECT_FALSE(HasInflightRequests());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasInflightRequests());
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.response_data());
  request_.reset();
  EXPECT_FALSE(HasInflightRequests());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_DelayedRegistration) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  SetUpWithHelper(new StreamResponder(kProcessID, stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("http://example.com/foo.html"),
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      nullptr);
  request_->set_method("GET");
  request_->Start();

  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  stream->Finalize();

  EXPECT_FALSE(HasInflightRequests());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasInflightRequests());
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.response_data());
  request_.reset();
  EXPECT_FALSE(HasInflightRequests());
}


TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_QuickFinalize) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  stream->Finalize();
  SetUpWithHelper(new StreamResponder(kProcessID, stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("http://example.com/foo.html"),
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      nullptr);
  request_->set_method("GET");
  request_->Start();
  EXPECT_FALSE(HasInflightRequests());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasInflightRequests());
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.response_data());
  request_.reset();
  EXPECT_FALSE(HasInflightRequests());
}


TEST_F(ServiceWorkerURLRequestJobTest, StreamResponse_Flush) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  SetUpWithHelper(new StreamResponder(kProcessID, stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("http://example.com/foo.html"),
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      nullptr);
  request_->set_method("GET");
  request_->Start();
  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 1024; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);;
    stream->Flush();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(expected_response, url_request_delegate_.response_data());
  }
  stream->Finalize();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_EQ(200,
            request_->response_headers()->response_code());
  EXPECT_EQ("OK",
            request_->response_headers()->GetStatusText());
  EXPECT_EQ(expected_response, url_request_delegate_.response_data());
}

TEST_F(ServiceWorkerURLRequestJobTest, StreamResponseAndCancel) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  ASSERT_EQ(stream.get(),
            stream_context->registry()->GetStream(stream_url).get());
  SetUpWithHelper(new StreamResponder(kProcessID, stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("http://example.com/foo.html"),
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      nullptr);
  request_->set_method("GET");
  request_->Start();
  EXPECT_FALSE(HasInflightRequests());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasInflightRequests());

  std::string expected_response;
  expected_response.reserve((sizeof(kTestData) - 1) * 1024);
  for (int i = 0; i < 512; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  ASSERT_TRUE(stream_context->registry()->GetStream(stream_url).get());
  request_->Cancel();
  EXPECT_FALSE(HasInflightRequests());
  ASSERT_FALSE(stream_context->registry()->GetStream(stream_url).get());
  for (int i = 0; i < 512; ++i) {
    expected_response += kTestData;
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  }
  stream->Finalize();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_->status().is_success());
}

TEST_F(ServiceWorkerURLRequestJobTest,
       StreamResponse_DelayedRegistrationAndCancel) {
  const GURL stream_url("blob://stream");
  StreamContext* stream_context =
      GetStreamContextForResourceContext(
          browser_context_->GetResourceContext());
  SetUpWithHelper(new StreamResponder(kProcessID, stream_url));

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  request_ = url_request_context_.CreateRequest(
      GURL("http://example.com/foo.html"),
      net::DEFAULT_PRIORITY,
      &url_request_delegate_,
      nullptr);
  request_->set_method("GET");
  request_->Start();
  EXPECT_FALSE(HasInflightRequests());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasInflightRequests());
  request_->Cancel();
  EXPECT_FALSE(HasInflightRequests());

  scoped_refptr<Stream> stream =
      new Stream(stream_context->registry(), nullptr, stream_url);
  // The stream should not be registered to the stream registry.
  ASSERT_FALSE(stream_context->registry()->GetStream(stream_url).get());
  for (int i = 0; i < 1024; ++i)
    stream->AddData(kTestData, sizeof(kTestData) - 1);
  stream->Finalize();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_->status().is_success());
}

// TODO(kinuko): Add more tests with different response data and also for
// FallbackToNetwork case.

}  // namespace content

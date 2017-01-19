// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/mojo_async_resource_handler.h"

#include <string.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "content/browser/loader/mock_resource_loader.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/test_url_loader_client.h"
#include "content/common/resource_request_completion_status.h"
#include "content/common/url_loader.mojom.h"
#include "content/public/browser/appcache_service.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/stream_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace content {
namespace {

constexpr int kSizeMimeSnifferRequiresForFirstOnWillRead = 2048;

class TestResourceDispatcherHostDelegate final
    : public ResourceDispatcherHostDelegate {
 public:
  TestResourceDispatcherHostDelegate() = default;
  ~TestResourceDispatcherHostDelegate() override {
    EXPECT_EQ(num_on_response_started_calls_expectation_,
              num_on_response_started_calls_);
  }

  bool ShouldBeginRequest(const std::string& method,
                          const GURL& url,
                          ResourceType resource_type,
                          ResourceContext* resource_context) override {
    ADD_FAILURE() << "ShouldBeginRequest should not be called.";
    return false;
  }

  void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      AppCacheService* appcache_service,
      ResourceType resource_type,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles) override {
    ADD_FAILURE() << "RequestBeginning should not be called.";
  }

  void DownloadStarting(
      net::URLRequest* request,
      ResourceContext* resource_context,
      bool is_content_initiated,
      bool must_download,
      bool is_new_request,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles) override {
    ADD_FAILURE() << "DownloadStarting should not be called.";
  }

  ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      net::URLRequest* request) override {
    ADD_FAILURE() << "CreateLoginDelegate should not be called.";
    return nullptr;
  }

  bool HandleExternalProtocol(
      const GURL& url,
      ResourceRequestInfo* resource_request_info) override {
    ADD_FAILURE() << "HandleExternalProtocol should not be called.";
    return false;
  }

  bool ShouldForceDownloadResource(const GURL& url,
                                   const std::string& mime_type) override {
    ADD_FAILURE() << "ShouldForceDownloadResource should not be called.";
    return false;
  }

  bool ShouldInterceptResourceAsStream(net::URLRequest* request,
                                       const base::FilePath& plugin_path,
                                       const std::string& mime_type,
                                       GURL* origin,
                                       std::string* payload) override {
    ADD_FAILURE() << "ShouldInterceptResourceAsStream should not be called.";
    return false;
  }

  void OnStreamCreated(net::URLRequest* request,
                       std::unique_ptr<content::StreamInfo> stream) override {
    ADD_FAILURE() << "OnStreamCreated should not be called.";
  }

  void OnResponseStarted(net::URLRequest* request,
                         ResourceContext* resource_context,
                         ResourceResponse* response) override {
    ++num_on_response_started_calls_;
  }

  void OnRequestRedirected(const GURL& redirect_url,
                           net::URLRequest* request,
                           ResourceContext* resource_context,
                           ResourceResponse* response) override {
    ADD_FAILURE() << "OnRequestRedirected should not be called.";
  }

  void RequestComplete(net::URLRequest* url_request) override {
    ADD_FAILURE() << "RequestComplete should not be called.";
  }

  PreviewsState GetPreviewsState(
      const net::URLRequest& url_request,
      content::ResourceContext* resource_context) override {
    ADD_FAILURE() << "GetPreviewsState should not be called.";
    return PREVIEWS_UNSPECIFIED;
  }

  NavigationData* GetNavigationData(net::URLRequest* request) const override {
    ADD_FAILURE() << "GetNavigationData should not be called.";
    return nullptr;
  }

  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      ResourceContext* resource_context) override {
    ADD_FAILURE() << "CreateClientCertStore should not be called.";
    return nullptr;
  }

  int num_on_response_started_calls() const {
    return num_on_response_started_calls_;
  }
  void set_num_on_response_started_calls_expectation(int expectation) {
    num_on_response_started_calls_expectation_ = expectation;
  }

 private:
  int num_on_response_started_calls_ = 0;
  int num_on_response_started_calls_expectation_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherHostDelegate);
};

class MojoAsyncResourceHandlerWithStubOperations
    : public MojoAsyncResourceHandler {
 public:
  MojoAsyncResourceHandlerWithStubOperations(
      net::URLRequest* request,
      ResourceDispatcherHostImpl* rdh,
      mojom::URLLoaderAssociatedRequest mojo_request,
      mojom::URLLoaderClientAssociatedPtr url_loader_client)
      : MojoAsyncResourceHandler(request,
                                 rdh,
                                 std::move(mojo_request),
                                 std::move(url_loader_client)) {}
  ~MojoAsyncResourceHandlerWithStubOperations() override {}

  void ResetBeginWriteExpectation() { is_begin_write_expectation_set_ = false; }

  void set_begin_write_expectation(MojoResult begin_write_expectation) {
    is_begin_write_expectation_set_ = true;
    begin_write_expectation_ = begin_write_expectation;
  }
  void set_end_write_expectation(MojoResult end_write_expectation) {
    is_end_write_expectation_set_ = true;
    end_write_expectation_ = end_write_expectation;
  }
  bool has_received_bad_message() const { return has_received_bad_message_; }
  void SetMetadata(scoped_refptr<net::IOBufferWithSize> metadata) {
    metadata_ = std::move(metadata);
  }

 private:
  MojoResult BeginWrite(void** data, uint32_t* available) override {
    if (is_begin_write_expectation_set_)
      return begin_write_expectation_;
    return MojoAsyncResourceHandler::BeginWrite(data, available);
  }
  MojoResult EndWrite(uint32_t written) override {
    if (is_end_write_expectation_set_)
      return end_write_expectation_;
    return MojoAsyncResourceHandler::EndWrite(written);
  }
  net::IOBufferWithSize* GetResponseMetadata(
      net::URLRequest* request) override {
    return metadata_.get();
  }

  void ReportBadMessage(const std::string& error) override {
    has_received_bad_message_ = true;
  }

  bool is_begin_write_expectation_set_ = false;
  bool is_end_write_expectation_set_ = false;
  bool has_received_bad_message_ = false;
  MojoResult begin_write_expectation_ = MOJO_RESULT_UNKNOWN;
  MojoResult end_write_expectation_ = MOJO_RESULT_UNKNOWN;
  scoped_refptr<net::IOBufferWithSize> metadata_;

  DISALLOW_COPY_AND_ASSIGN(MojoAsyncResourceHandlerWithStubOperations);
};

class TestURLLoaderFactory final : public mojom::URLLoaderFactory {
 public:
  TestURLLoaderFactory() {}
  ~TestURLLoaderFactory() override {}

  void CreateLoaderAndStart(
      mojom::URLLoaderAssociatedRequest request,
      int32_t routing_id,
      int32_t request_id,
      const ResourceRequest& url_request,
      mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info) override {
    loader_request_ = std::move(request);
    client_ptr_info_ = std::move(client_ptr_info);
  }

  mojom::URLLoaderAssociatedRequest PassLoaderRequest() {
    return std::move(loader_request_);
  }

  mojom::URLLoaderClientAssociatedPtrInfo PassClientPtrInfo() {
    return std::move(client_ptr_info_);
  }

  void SyncLoad(int32_t routing_id,
                int32_t request_id,
                const ResourceRequest& url_request,
                const SyncLoadCallback& callback) override {
    NOTREACHED();
  }

 private:
  mojom::URLLoaderAssociatedRequest loader_request_;
  mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactory);
};

class MojoAsyncResourceHandlerTestBase {
 public:
  MojoAsyncResourceHandlerTestBase()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()) {
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(32 * 1024);
    rdh_.SetDelegate(&rdh_delegate_);

    // Create and initialize |request_|.  None of this matters, for these tests,
    // just need something non-NULL.
    net::URLRequestContext* request_context =
        browser_context_->GetResourceContext()->GetRequestContext();
    request_ = request_context->CreateRequest(
        GURL("http://foo/"), net::DEFAULT_PRIORITY, &url_request_delegate_);
    ResourceRequestInfo::AllocateForTesting(
        request_.get(),                          // request
        RESOURCE_TYPE_XHR,                       // resource_type
        browser_context_->GetResourceContext(),  // context
        2,                                       // render_process_id
        0,                                       // render_view_id
        0,                                       // render_frame_id
        true,                                    // is_main_frame
        false,                                   // parent_is_main_frame
        false,                                   // allow_download
        true,                                    // is_async
        PREVIEWS_OFF                             // previews_state
        );

    ResourceRequest request;
    base::WeakPtr<mojo::StrongBinding<mojom::URLLoaderFactory>> weak_binding =
        mojo::MakeStrongBinding(base::MakeUnique<TestURLLoaderFactory>(),
                                mojo::MakeRequest(&url_loader_factory_));

    url_loader_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader_proxy_,
                          url_loader_factory_.associated_group()),
        0, 0, request, url_loader_client_.CreateRemoteAssociatedPtrInfo(
                           url_loader_factory_.associated_group()));

    url_loader_factory_.FlushForTesting();
    DCHECK(weak_binding);
    TestURLLoaderFactory* factory_impl =
        static_cast<TestURLLoaderFactory*>(weak_binding->impl());

    mojom::URLLoaderClientAssociatedPtr client_ptr;
    client_ptr.Bind(factory_impl->PassClientPtrInfo());
    handler_.reset(new MojoAsyncResourceHandlerWithStubOperations(
        request_.get(), &rdh_, factory_impl->PassLoaderRequest(),
        std::move(client_ptr)));
    mock_loader_.reset(new MockResourceLoader(handler_.get()));
  }

  virtual ~MojoAsyncResourceHandlerTestBase() {
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(
        MojoAsyncResourceHandler::kDefaultAllocationSize);
    base::RunLoop().RunUntilIdle();
  }

  // Returns false if something bad happens.
  bool CallOnWillStartAndOnResponseStarted() {
    rdh_delegate_.set_num_on_response_started_calls_expectation(1);
    MockResourceLoader::Status result =
        mock_loader_->OnWillStart(request_->url());
    EXPECT_EQ(MockResourceLoader::Status::IDLE, result);
    if (result != MockResourceLoader::Status::IDLE)
      return false;

    result = mock_loader_->OnResponseStarted(
        make_scoped_refptr(new ResourceResponse()));
    EXPECT_EQ(MockResourceLoader::Status::IDLE, result);
    if (result != MockResourceLoader::Status::IDLE)
      return false;

    if (url_loader_client_.has_received_response()) {
      ADD_FAILURE() << "URLLoaderClient unexpectedly gets a response.";
      return false;
    }
    url_loader_client_.RunUntilResponseReceived();
    return true;
  }

  TestBrowserThreadBundle thread_bundle_;
  TestResourceDispatcherHostDelegate rdh_delegate_;
  ResourceDispatcherHostImpl rdh_;
  mojom::URLLoaderFactoryPtr url_loader_factory_;
  mojom::URLLoaderAssociatedPtr url_loader_proxy_;
  TestURLLoaderClient url_loader_client_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  net::TestDelegate url_request_delegate_;
  std::unique_ptr<net::URLRequest> request_;
  std::unique_ptr<MojoAsyncResourceHandlerWithStubOperations> handler_;
  std::unique_ptr<MockResourceLoader> mock_loader_;

  DISALLOW_COPY_AND_ASSIGN(MojoAsyncResourceHandlerTestBase);
};

class MojoAsyncResourceHandlerTest : public MojoAsyncResourceHandlerTestBase,
                                     public ::testing::Test {};

// This test class is parameterized with MojoAsyncResourceHandler's allocation
// size.
class MojoAsyncResourceHandlerWithAllocationSizeTest
    : public MojoAsyncResourceHandlerTestBase,
      public ::testing::TestWithParam<size_t> {
 protected:
  MojoAsyncResourceHandlerWithAllocationSizeTest() {
    MojoAsyncResourceHandler::SetAllocationSizeForTesting(GetParam());
  }
};

TEST_F(MojoAsyncResourceHandlerTest, InFlightRequests) {
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
  handler_ = nullptr;
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
}

TEST_F(MojoAsyncResourceHandlerTest, OnWillStart) {
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));
}

TEST_F(MojoAsyncResourceHandlerTest, OnResponseStarted) {
  rdh_delegate_.set_num_on_response_started_calls_expectation(1);
  scoped_refptr<net::IOBufferWithSize> metadata = new net::IOBufferWithSize(5);
  memcpy(metadata->data(), "hello", 5);
  handler_->SetMetadata(metadata);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  scoped_refptr<ResourceResponse> response = new ResourceResponse();
  response->head.content_length = 99;
  response->head.request_start =
      base::TimeTicks::UnixEpoch() + base::TimeDelta::FromDays(14);
  response->head.response_start =
      base::TimeTicks::UnixEpoch() + base::TimeDelta::FromDays(28);

  EXPECT_EQ(0, rdh_delegate_.num_on_response_started_calls());
  base::TimeTicks now1 = base::TimeTicks::Now();
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(response));
  base::TimeTicks now2 = base::TimeTicks::Now();

  EXPECT_EQ(request_->creation_time(), response->head.request_start);
  EXPECT_LE(now1, response->head.response_start);
  EXPECT_LE(response->head.response_start, now2);
  EXPECT_EQ(1, rdh_delegate_.num_on_response_started_calls());

  url_loader_client_.RunUntilResponseReceived();
  EXPECT_EQ(response->head.request_start,
            url_loader_client_.response_head().request_start);
  EXPECT_EQ(response->head.response_start,
            url_loader_client_.response_head().response_start);
  EXPECT_EQ(99, url_loader_client_.response_head().content_length);

  url_loader_client_.RunUntilCachedMetadataReceived();
  EXPECT_EQ("hello", url_loader_client_.cached_metadata());
}

TEST_F(MojoAsyncResourceHandlerTest, OnWillReadAndInFlightRequests) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
  EXPECT_EQ(1, rdh_.num_in_flight_requests_for_testing());
  handler_ = nullptr;
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
}

TEST_F(MojoAsyncResourceHandlerTest, OnWillReadWithInsufficientResource) {
  rdh_.set_max_num_in_flight_requests_per_process(0);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->OnWillRead(-1));
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES, mock_loader_->error_code());
  EXPECT_EQ(1, rdh_.num_in_flight_requests_for_testing());
  handler_ = nullptr;
  EXPECT_EQ(0, rdh_.num_in_flight_requests_for_testing());
}

TEST_F(MojoAsyncResourceHandlerTest, OnWillReadAndOnReadCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
  // The buffer size that the mime sniffer requires implicitly.
  ASSERT_GE(mock_loader_->io_buffer_size(),
            kSizeMimeSnifferRequiresForFirstOnWillRead);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnReadCompleted("AB"));

  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  std::string contents;
  while (contents.size() < 2) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buffer,
                          &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
      continue;
    }
    contents.append(buffer, read_size);
  }
  EXPECT_EQ("AB", contents);
}

TEST_F(MojoAsyncResourceHandlerTest,
       OnWillReadAndOnReadCompletedWithInsufficientInitialCapacity) {
  MojoAsyncResourceHandler::SetAllocationSizeForTesting(2);

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
  // The buffer size that the mime sniffer requires implicitly.
  ASSERT_GE(mock_loader_->io_buffer_size(),
            kSizeMimeSnifferRequiresForFirstOnWillRead);

  const std::string data("abcdefgh");
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnReadCompleted(data));

  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  std::string contents;
  while (contents.size() < data.size()) {
    // This is needed for Resume to be called.
    base::RunLoop().RunUntilIdle();
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buffer,
                          &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT)
      continue;
    ASSERT_EQ(MOJO_RESULT_OK, result);
    contents.append(buffer, read_size);
  }
  EXPECT_EQ(data, contents);
  EXPECT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->status());
}

TEST_F(MojoAsyncResourceHandlerTest,
       IOBufferFromOnWillReadShouldRemainValidEvenIfHandlerIsGone) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
  // The io_buffer size that the mime sniffer requires implicitly.
  ASSERT_GE(mock_loader_->io_buffer_size(),
            kSizeMimeSnifferRequiresForFirstOnWillRead);

  handler_ = nullptr;
  url_loader_client_.Unbind();
  base::RunLoop().RunUntilIdle();

  // Hopefully ASAN checks this operation's validity.
  mock_loader_->io_buffer()->data()[0] = 'A';
}

TEST_F(MojoAsyncResourceHandlerTest, OnResponseCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  ResourceRequestInfoImpl::ForRequest(request_.get())
      ->set_was_ignored_by_handler(false);
  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);

  base::TimeTicks now1 = base::TimeTicks::Now();
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));
  base::TimeTicks now2 = base::TimeTicks::Now();

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);
  EXPECT_FALSE(url_loader_client_.completion_status().was_ignored_by_handler);
  EXPECT_LE(now1, url_loader_client_.completion_status().completion_time);
  EXPECT_LE(url_loader_client_.completion_status().completion_time, now2);
  EXPECT_EQ(request_->GetTotalReceivedBytes(),
            url_loader_client_.completion_status().encoded_data_length);
}

// This test case sets different status values from OnResponseCompleted.
TEST_F(MojoAsyncResourceHandlerTest, OnResponseCompleted2) {
  rdh_.SetDelegate(nullptr);
  // Don't use CallOnWillStartAndOnResponseStarted as this test case manually
  // sets the null delegate.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                make_scoped_refptr(new ResourceResponse())));
  ASSERT_FALSE(url_loader_client_.has_received_response());
  url_loader_client_.RunUntilResponseReceived();

  ResourceRequestInfoImpl::ForRequest(request_.get())
      ->set_was_ignored_by_handler(true);
  net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                               net::ERR_ABORTED);

  base::TimeTicks now1 = base::TimeTicks::Now();
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));
  base::TimeTicks now2 = base::TimeTicks::Now();

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_ABORTED,
            url_loader_client_.completion_status().error_code);
  EXPECT_TRUE(url_loader_client_.completion_status().was_ignored_by_handler);
  EXPECT_LE(now1, url_loader_client_.completion_status().completion_time);
  EXPECT_LE(url_loader_client_.completion_status().completion_time, now2);
  EXPECT_EQ(request_->GetTotalReceivedBytes(),
            url_loader_client_.completion_status().encoded_data_length);
}

TEST_F(MojoAsyncResourceHandlerTest, OnResponseCompletedWithCanceledTimedOut) {
  net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                               net::ERR_TIMED_OUT);

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_TIMED_OUT,
            url_loader_client_.completion_status().error_code);
}

TEST_F(MojoAsyncResourceHandlerTest, OnResponseCompletedWithFailedTimedOut) {
  net::URLRequestStatus status(net::URLRequestStatus::FAILED,
                               net::ERR_TIMED_OUT);

  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_TIMED_OUT,
            url_loader_client_.completion_status().error_code);
}

TEST_F(MojoAsyncResourceHandlerTest, ResponseCompletionShouldCloseDataPipe) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnReadCompleted("AB"));
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);

  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buffer,
                          &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    ASSERT_TRUE(result == MOJO_RESULT_SHOULD_WAIT || result == MOJO_RESULT_OK);
  }
}

TEST_F(MojoAsyncResourceHandlerTest, OutOfBandCancelDuringBodyTransmission) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
  std::string data(mock_loader_->io_buffer_size(), 'a');
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnReadCompleted(data));
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::FAILED, net::ERR_FAILED);
  ASSERT_EQ(
      MockResourceLoader::Status::IDLE,
      mock_loader_->OnResponseCompletedFromExternalOutOfBandCancel(status));

  url_loader_client_.RunUntilComplete();
  EXPECT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, url_loader_client_.completion_status().error_code);

  std::string actual;
  while (true) {
    char buf[16];
    uint32_t read_size = sizeof(buf);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buf, &read_size,
                          MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
      continue;
    }
    EXPECT_EQ(MOJO_RESULT_OK, result);
    actual.append(buf, read_size);
  }
  EXPECT_EQ(data, actual);
}

TEST_F(MojoAsyncResourceHandlerTest, BeginWriteFailsOnWillRead) {
  handler_->set_begin_write_expectation(MOJO_RESULT_UNKNOWN);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->OnWillRead(-1));
}

TEST_F(MojoAsyncResourceHandlerTest, BeginWriteReturnsShouldWaitOnWillRead) {
  handler_->set_begin_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
}

TEST_F(MojoAsyncResourceHandlerTest,
       BeginWriteReturnsShouldWaitOnWillReadAndThenReturnsOK) {
  handler_->set_begin_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  size_t written = 0;
  while (true) {
    ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
    MockResourceLoader::Status result = mock_loader_->OnReadCompleted(
        std::string(mock_loader_->io_buffer_size(), 'X'));
    written += mock_loader_->io_buffer_size();
    if (result == MockResourceLoader::Status::CALLBACK_PENDING)
      break;
    ASSERT_EQ(MockResourceLoader::Status::IDLE, result);
  }

  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());
  handler_->ResetBeginWriteExpectation();
  handler_->OnWritableForTesting();

  std::string actual;
  while (actual.size() < written) {
    char buf[16];
    uint32_t read_size = sizeof(buf);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buf, &read_size,
                          MOJO_READ_DATA_FLAG_NONE);
    ASSERT_TRUE(result == MOJO_RESULT_OK || result == MOJO_RESULT_SHOULD_WAIT);
    if (result == MOJO_RESULT_OK)
      actual.append(buf, read_size);
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(std::string(written, 'X'), actual);
  EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());
}

TEST_F(MojoAsyncResourceHandlerTest,
       EndWriteFailsOnWillReadWithInsufficientInitialCapacity) {
  MojoAsyncResourceHandler::SetAllocationSizeForTesting(2);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  handler_->set_end_write_expectation(MOJO_RESULT_UNKNOWN);
  ASSERT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->OnWillRead(-1));
}

TEST_F(MojoAsyncResourceHandlerTest, EndWriteFailsOnReadCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));

  handler_->set_end_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  ASSERT_EQ(MockResourceLoader::Status::CANCELED,
            mock_loader_->OnReadCompleted(
                std::string(mock_loader_->io_buffer_size(), 'w')));
}

TEST_F(MojoAsyncResourceHandlerTest,
       EndWriteFailsOnReadCompletedWithInsufficientInitialCapacity) {
  MojoAsyncResourceHandler::SetAllocationSizeForTesting(2);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));

  handler_->set_end_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  ASSERT_EQ(MockResourceLoader::Status::CANCELED,
            mock_loader_->OnReadCompleted(
                std::string(mock_loader_->io_buffer_size(), 'w')));
}

TEST_F(MojoAsyncResourceHandlerTest,
       EndWriteFailsOnResumeWithInsufficientInitialCapacity) {
  MojoAsyncResourceHandler::SetAllocationSizeForTesting(8);
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));

  while (true) {
    MockResourceLoader::Status result = mock_loader_->OnReadCompleted(
        std::string(mock_loader_->io_buffer_size(), 'A'));
    if (result == MockResourceLoader::Status::CALLBACK_PENDING)
      break;
    ASSERT_EQ(MockResourceLoader::Status::IDLE, result);

    ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
  }

  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  while (true) {
    char buf[16];
    uint32_t read_size = sizeof(buf);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buf, &read_size,
                          MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT)
      break;
    ASSERT_EQ(MOJO_RESULT_OK, result);
  }

  handler_->set_end_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  mock_loader_->WaitUntilIdleOrCanceled();
  EXPECT_FALSE(url_loader_client_.has_received_completion());
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader_->status());
  EXPECT_EQ(net::ERR_FAILED, mock_loader_->error_code());
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       OnWillReadWithLongContents) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
  std::string expected;
  for (int i = 0; i < 3 * mock_loader_->io_buffer_size() + 2; ++i)
    expected += ('A' + i % 26);

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnReadCompleted(0));

  size_t written = 0;
  std::string actual;
  while (actual.size() < expected.size()) {
    while (written < expected.size() &&
           mock_loader_->status() == MockResourceLoader::Status::IDLE) {
      ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
      const size_t to_be_written =
          std::min(static_cast<size_t>(mock_loader_->io_buffer_size()),
                   expected.size() - written);

      // Request should be resumed or paused.
      ASSERT_NE(MockResourceLoader::Status::CANCELED,
                mock_loader_->OnReadCompleted(
                    expected.substr(written, to_be_written)));

      written += to_be_written;
    }
    if (!url_loader_client_.response_body().is_valid()) {
      url_loader_client_.RunUntilResponseBodyArrived();
      ASSERT_TRUE(url_loader_client_.response_body().is_valid());
    }

    char buf[16];
    uint32_t read_size = sizeof(buf);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buf, &read_size,
                          MOJO_READ_DATA_FLAG_NONE);
    if (result != MOJO_RESULT_SHOULD_WAIT) {
      ASSERT_EQ(MOJO_RESULT_OK, result);
      actual.append(buf, read_size);
    }

    // Give mojo a chance pass data back and forth, and to request more data
    // from the handler.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(expected, actual);
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       BeginWriteFailsOnReadCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));

  handler_->set_begin_write_expectation(MOJO_RESULT_UNKNOWN);
  ASSERT_EQ(MockResourceLoader::Status::CANCELED,
            mock_loader_->OnReadCompleted(
                std::string(mock_loader_->io_buffer_size(), 'A')));
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       BeginWriteReturnsShouldWaitOnReadCompleted) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));

  handler_->set_begin_write_expectation(MOJO_RESULT_SHOULD_WAIT);
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnReadCompleted(
                std::string(mock_loader_->io_buffer_size(), 'A')));
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       BeginWriteFailsOnResume) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnReadCompleted(0));

  while (true) {
    ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
    MockResourceLoader::Status result = mock_loader_->OnReadCompleted(
        std::string(mock_loader_->io_buffer_size(), 'A'));
    if (result == MockResourceLoader::Status::CALLBACK_PENDING)
      break;
    ASSERT_EQ(MockResourceLoader::Status::IDLE, result);
  }
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  handler_->set_begin_write_expectation(MOJO_RESULT_UNKNOWN);

  while (mock_loader_->status() != MockResourceLoader::Status::CANCELED) {
    char buf[256];
    uint32_t read_size = sizeof(buf);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buf, &read_size,
                          MOJO_READ_DATA_FLAG_NONE);
    ASSERT_TRUE(result == MOJO_RESULT_OK || result == MOJO_RESULT_SHOULD_WAIT);
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, mock_loader_->error_code());
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest, CancelWhileWaiting) {
  ASSERT_TRUE(CallOnWillStartAndOnResponseStarted());

  while (true) {
    ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));
    MockResourceLoader::Status result = mock_loader_->OnReadCompleted(
        std::string(mock_loader_->io_buffer_size(), 'A'));
    if (result == MockResourceLoader::Status::CALLBACK_PENDING)
      break;
    ASSERT_EQ(MockResourceLoader::Status::IDLE, result);
  }

  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                               net::ERR_ABORTED);
  ASSERT_EQ(
      MockResourceLoader::Status::IDLE,
      mock_loader_->OnResponseCompletedFromExternalOutOfBandCancel(status));

  ASSERT_FALSE(url_loader_client_.has_received_completion());
  url_loader_client_.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED,
            url_loader_client_.completion_status().error_code);

  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buffer,
                          &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    base::RunLoop().RunUntilIdle();
    DCHECK(result == MOJO_RESULT_SHOULD_WAIT || result == MOJO_RESULT_OK);
  }

  base::RunLoop().RunUntilIdle();
}

TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest, RedirectHandling) {
  rdh_delegate_.set_num_on_response_started_calls_expectation(1);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnRequestRedirected(
                redirect_info, make_scoped_refptr(new ResourceResponse())));

  ASSERT_FALSE(url_loader_client_.has_received_response());
  ASSERT_FALSE(url_loader_client_.has_received_redirect());
  url_loader_client_.RunUntilRedirectReceived();

  ASSERT_FALSE(url_loader_client_.has_received_response());
  ASSERT_TRUE(url_loader_client_.has_received_redirect());
  EXPECT_EQ(301, url_loader_client_.redirect_info().status_code);

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->status());
  handler_->FollowRedirect();
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

  url_loader_client_.ClearHasReceivedRedirect();
  // Redirect once more.
  redirect_info.status_code = 302;
  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->OnRequestRedirected(
                redirect_info, make_scoped_refptr(new ResourceResponse())));

  ASSERT_FALSE(url_loader_client_.has_received_response());
  ASSERT_FALSE(url_loader_client_.has_received_redirect());
  url_loader_client_.RunUntilRedirectReceived();

  ASSERT_FALSE(url_loader_client_.has_received_response());
  ASSERT_TRUE(url_loader_client_.has_received_redirect());
  EXPECT_EQ(302, url_loader_client_.redirect_info().status_code);

  ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
            mock_loader_->status());
  handler_->FollowRedirect();
  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->status());

  // Give the final response.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                make_scoped_refptr(new ResourceResponse())));

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  ASSERT_FALSE(url_loader_client_.has_received_completion());
  url_loader_client_.RunUntilComplete();

  ASSERT_TRUE(url_loader_client_.has_received_response());
  ASSERT_TRUE(url_loader_client_.has_received_completion());
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);
}

// Test the case where th other process tells the ResourceHandler to follow a
// redirect, despite the fact that no redirect has been received yet.
TEST_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
       MalformedFollowRedirectRequest) {
  handler_->FollowRedirect();

  EXPECT_TRUE(handler_->has_received_bad_message());
}

// Typically ResourceHandler methods are called in this order.
TEST_P(
    MojoAsyncResourceHandlerWithAllocationSizeTest,
    OnWillStartThenOnResponseStartedThenOnWillReadThenOnReadCompletedThenOnResponseCompleted) {
  rdh_delegate_.set_num_on_response_started_calls_expectation(1);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                make_scoped_refptr(new ResourceResponse())));

  ASSERT_FALSE(url_loader_client_.has_received_response());
  url_loader_client_.RunUntilResponseReceived();

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));

  ASSERT_FALSE(url_loader_client_.response_body().is_valid());

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnReadCompleted("A"));
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  ASSERT_FALSE(url_loader_client_.has_received_completion());
  url_loader_client_.RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);

  std::string body;
  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buffer,
                          &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
    } else {
      ASSERT_EQ(result, MOJO_RESULT_OK);
      body.append(buffer, read_size);
    }
  }
  EXPECT_EQ("A", body);
}

// MimeResourceHandler calls delegated ResourceHandler's methods in this order.
TEST_P(
    MojoAsyncResourceHandlerWithAllocationSizeTest,
    OnWillStartThenOnWillReadThenOnResponseStartedThenOnReadCompletedThenOnResponseCompleted) {
  rdh_delegate_.set_num_on_response_started_calls_expectation(1);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnWillStart(request_->url()));

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader_->OnWillRead(-1));

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseStarted(
                make_scoped_refptr(new ResourceResponse())));

  ASSERT_FALSE(url_loader_client_.has_received_response());
  url_loader_client_.RunUntilResponseReceived();

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnReadCompleted("B"));

  ASSERT_FALSE(url_loader_client_.response_body().is_valid());
  url_loader_client_.RunUntilResponseBodyArrived();
  ASSERT_TRUE(url_loader_client_.response_body().is_valid());

  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, net::OK);
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader_->OnResponseCompleted(status));

  ASSERT_FALSE(url_loader_client_.has_received_completion());
  url_loader_client_.RunUntilComplete();
  EXPECT_EQ(net::OK, url_loader_client_.completion_status().error_code);

  std::string body;
  while (true) {
    char buffer[16];
    uint32_t read_size = sizeof(buffer);
    MojoResult result =
        mojo::ReadDataRaw(url_loader_client_.response_body(), buffer,
                          &read_size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      break;
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop().RunUntilIdle();
    } else {
      ASSERT_EQ(result, MOJO_RESULT_OK);
      body.append(buffer, read_size);
    }
  }
  EXPECT_EQ("B", body);
}

INSTANTIATE_TEST_CASE_P(MojoAsyncResourceHandlerWithAllocationSizeTest,
                        MojoAsyncResourceHandlerWithAllocationSizeTest,
                        ::testing::Values(8, 32 * 2014));
}  // namespace
}  // namespace content

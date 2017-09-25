// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/resource_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/child/request_extra_data.h"
#include "content/child/test_request_peer.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/resource_messages.h"
#include "content/public/child/fixed_received_data.h"
#include "content/public/child/request_peer.h"
#include "content/public/child/resource_dispatcher_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/request_context_frame_type.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/common/resource_request_completion_status.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/service_worker_modes.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "url/gurl.h"

namespace content {

static const char kTestPageUrl[] = "http://www.google.com/";
static const char kTestPageHeaders[] =
  "HTTP/1.1 200 OK\nContent-Type:text/html\n\n";
static const char kTestPageMimeType[] = "text/html";
static const char kTestPageCharset[] = "";
static const char kTestPageContents[] =
  "<html><head><title>Google</title></head><body><h1>Google</h1></body></html>";
static const char kTestRedirectHeaders[] =
  "HTTP/1.1 302 Found\nLocation:http://www.google.com/\n\n";

// Sets up the message sender override for the unit test.
class ResourceDispatcherTest : public testing::Test, public IPC::Sender {
 public:
  ResourceDispatcherTest()
      : dispatcher_(new ResourceDispatcher(this, message_loop_.task_runner())) {
  }

  ~ResourceDispatcherTest() override {
    shared_memory_map_.clear();
    dispatcher_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Emulates IPC send operations (IPC::Sender) by adding
  // pending messages to the queue.
  bool Send(IPC::Message* msg) override {
    message_queue_.push_back(IPC::Message(*msg));
    delete msg;
    return true;
  }

  size_t queued_messages() const { return message_queue_.size(); }

  // Returns the ID of the consumed request.  Can't make assumptions about the
  // ID, because numbering is based on a global.
  int ConsumeRequestResource() {
    if (message_queue_.empty()) {
      ADD_FAILURE() << "Missing resource request message";
      return -1;
    }

    ResourceHostMsg_RequestResource::Param params;
    if (ResourceHostMsg_RequestResource::ID != message_queue_[0].type() ||
        !ResourceHostMsg_RequestResource::Read(&message_queue_[0], &params)) {
      ADD_FAILURE() << "Expected ResourceHostMsg_RequestResource message";
      return -1;
    }
    ResourceRequest request = std::get<2>(params);
    EXPECT_EQ(kTestPageUrl, request.url.spec());
    message_queue_.erase(message_queue_.begin());
    return std::get<1>(params);
  }

  void ConsumeFollowRedirect(int expected_request_id) {
    ASSERT_FALSE(message_queue_.empty());
    std::tuple<int> args;
    ASSERT_EQ(ResourceHostMsg_FollowRedirect::ID, message_queue_[0].type());
    ASSERT_TRUE(ResourceHostMsg_FollowRedirect::Read(
        &message_queue_[0], &args));
    EXPECT_EQ(expected_request_id, std::get<0>(args));
    message_queue_.erase(message_queue_.begin());
  }

  void ConsumeDataReceived_ACK(int expected_request_id) {
    ASSERT_FALSE(message_queue_.empty());
    std::tuple<int> args;
    ASSERT_EQ(ResourceHostMsg_DataReceived_ACK::ID, message_queue_[0].type());
    ASSERT_TRUE(ResourceHostMsg_DataReceived_ACK::Read(
        &message_queue_[0], &args));
    EXPECT_EQ(expected_request_id, std::get<0>(args));
    message_queue_.erase(message_queue_.begin());
  }

  void ConsumeReleaseDownloadedFile(int expected_request_id) {
    ASSERT_FALSE(message_queue_.empty());
    std::tuple<int> args;
    ASSERT_EQ(ResourceHostMsg_ReleaseDownloadedFile::ID,
              message_queue_[0].type());
    ASSERT_TRUE(ResourceHostMsg_ReleaseDownloadedFile::Read(
        &message_queue_[0], &args));
    EXPECT_EQ(expected_request_id, std::get<0>(args));
    message_queue_.erase(message_queue_.begin());
  }

  void ConsumeCancelRequest(int expected_request_id) {
    ASSERT_FALSE(message_queue_.empty());
    std::tuple<int> args;
    ASSERT_EQ(ResourceHostMsg_CancelRequest::ID, message_queue_[0].type());
    ASSERT_TRUE(ResourceHostMsg_CancelRequest::Read(
        &message_queue_[0], &args));
    EXPECT_EQ(expected_request_id, std::get<0>(args));
    message_queue_.erase(message_queue_.begin());
  }

  void NotifyReceivedRedirect(int request_id) {
    ResourceResponseHead head;
    std::string raw_headers(kTestRedirectHeaders);
    std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
    head.headers = new net::HttpResponseHeaders(raw_headers);
    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = GURL(kTestPageUrl);
    redirect_info.new_site_for_cookies = GURL(kTestPageUrl);
    EXPECT_EQ(true, dispatcher_->OnMessageReceived(ResourceMsg_ReceivedRedirect(
                        request_id, redirect_info, head)));
  }

  void NotifyReceivedResponse(int request_id) {
    ResourceResponseHead head;
    std::string raw_headers(kTestPageHeaders);
    std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
    head.headers = new net::HttpResponseHeaders(raw_headers);
    head.mime_type = kTestPageMimeType;
    head.charset = kTestPageCharset;
    EXPECT_EQ(true, dispatcher_->OnMessageReceived(
                        ResourceMsg_ReceivedResponse(request_id, head)));
  }

  void NotifySetDataBuffer(int request_id, size_t buffer_size) {
    base::SharedMemory* shared_memory = new base::SharedMemory();
    ASSERT_FALSE(shared_memory_map_[request_id]);
    shared_memory_map_[request_id] = base::WrapUnique(shared_memory);
    EXPECT_TRUE(shared_memory->CreateAndMapAnonymous(buffer_size));

    base::SharedMemoryHandle duplicate_handle =
        shared_memory->handle().Duplicate();
    EXPECT_TRUE(duplicate_handle.IsValid());
    EXPECT_TRUE(dispatcher_->OnMessageReceived(ResourceMsg_SetDataBuffer(
        request_id, duplicate_handle, shared_memory->requested_size(), 0)));
  }

  void NotifyDataReceived(int request_id, const std::string& data) {
    ASSERT_LE(data.length(), shared_memory_map_[request_id]->requested_size());
    memcpy(shared_memory_map_[request_id]->memory(), data.c_str(),
           data.length());

    EXPECT_TRUE(dispatcher_->OnMessageReceived(ResourceMsg_DataReceived(
        request_id, 0, data.length(), data.length())));
  }

  void NotifyDataDownloaded(int request_id,
                            int decoded_length,
                            int encoded_data_length) {
    EXPECT_TRUE(dispatcher_->OnMessageReceived(ResourceMsg_DataDownloaded(
        request_id, decoded_length, encoded_data_length)));
  }

  void NotifyRequestComplete(int request_id, size_t total_size) {
    ResourceRequestCompletionStatus request_complete_data;
    request_complete_data.error_code = net::OK;
    request_complete_data.exists_in_cache = false;
    request_complete_data.encoded_data_length = total_size;
    EXPECT_TRUE(dispatcher_->OnMessageReceived(
        ResourceMsg_RequestComplete(request_id, request_complete_data)));
  }

  std::unique_ptr<ResourceRequest> CreateResourceRequest(
      bool download_to_file) {
    std::unique_ptr<ResourceRequest> request(new ResourceRequest());

    request->method = "GET";
    request->url = GURL(kTestPageUrl);
    request->site_for_cookies = GURL(kTestPageUrl);
    request->referrer_policy = blink::kWebReferrerPolicyDefault;
    request->resource_type = RESOURCE_TYPE_SUB_RESOURCE;
    request->priority = net::LOW;
    request->fetch_request_mode = FETCH_REQUEST_MODE_NO_CORS;
    request->fetch_frame_type = REQUEST_CONTEXT_FRAME_TYPE_NONE;
    request->download_to_file = download_to_file;

    const RequestExtraData extra_data;
    extra_data.CopyToResourceRequest(request.get());

    return request;
  }

  ResourceDispatcher* dispatcher() { return dispatcher_.get(); }

  int StartAsync(std::unique_ptr<ResourceRequest> request,
                 ResourceRequestBody* request_body,
                 TestRequestPeer::Context* peer_context) {
    std::unique_ptr<TestRequestPeer> peer(
        new TestRequestPeer(dispatcher(), peer_context));
    int request_id = dispatcher()->StartAsync(
        std::move(request), 0, nullptr, url::Origin(),
        TRAFFIC_ANNOTATION_FOR_TESTS, false, std::move(peer),
        blink::WebURLRequest::LoadingIPCType::kChromeIPC, nullptr,
        std::vector<std::unique_ptr<URLLoaderThrottle>>(),
        mojo::ScopedDataPipeConsumerHandle());
    peer_context->request_id = request_id;
    return request_id;
  }

 private:
  // Map of request IDs to shared memory.
  std::map<int, std::unique_ptr<base::SharedMemory>> shared_memory_map_;

  std::vector<IPC::Message> message_queue_;
  base::MessageLoop message_loop_;
  std::unique_ptr<ResourceDispatcher> dispatcher_;
};

// Tests the generation of unique request ids.
TEST_F(ResourceDispatcherTest, MakeRequestID) {
  int first_id = ResourceDispatcher::MakeRequestID();
  int second_id = ResourceDispatcher::MakeRequestID();

  // Child process ids are unique (per process) and counting from 0 upwards:
  EXPECT_GT(second_id, first_id);
  EXPECT_GE(first_id, 0);
}

// Does a simple request and tests that the correct data is received.  Simulates
// two reads.
TEST_F(ResourceDispatcherTest, RoundTrip) {
  // Number of bytes received in the first read.
  const size_t kFirstReceiveSize = 2;
  ASSERT_LT(kFirstReceiveSize, strlen(kTestPageContents));

  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  StartAsync(std::move(request), NULL, &peer_context);

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer_context.received_response);

  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, std::string(kTestPageContents, kFirstReceiveSize));
  ConsumeDataReceived_ACK(id);
  EXPECT_EQ(0u, queued_messages());

  NotifyDataReceived(id, kTestPageContents + kFirstReceiveSize);
  ConsumeDataReceived_ACK(id);
  EXPECT_EQ(0u, queued_messages());

  NotifyRequestComplete(id, strlen(kTestPageContents));
  EXPECT_EQ(kTestPageContents, peer_context.data);
  EXPECT_TRUE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());
}

// Tests that the request IDs are straight when there are two interleaving
// requests.
TEST_F(ResourceDispatcherTest, MultipleRequests) {
  const char kTestPageContents2[] = "Not kTestPageContents";

  std::unique_ptr<ResourceRequest> request1(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context1;
  StartAsync(std::move(request1), NULL, &peer_context1);

  std::unique_ptr<ResourceRequest> request2(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context2;
  StartAsync(std::move(request2), NULL, &peer_context2);

  int id1 = ConsumeRequestResource();
  int id2 = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id1);
  EXPECT_TRUE(peer_context1.received_response);
  EXPECT_FALSE(peer_context2.received_response);
  NotifyReceivedResponse(id2);
  EXPECT_TRUE(peer_context2.received_response);
  EXPECT_EQ(0u, queued_messages());

  NotifySetDataBuffer(id2, strlen(kTestPageContents2));
  NotifyDataReceived(id2, kTestPageContents2);
  ConsumeDataReceived_ACK(id2);
  NotifySetDataBuffer(id1, strlen(kTestPageContents));
  NotifyDataReceived(id1, kTestPageContents);
  ConsumeDataReceived_ACK(id1);
  EXPECT_EQ(0u, queued_messages());

  NotifyRequestComplete(id1, strlen(kTestPageContents));
  EXPECT_EQ(kTestPageContents, peer_context1.data);
  EXPECT_TRUE(peer_context1.complete);
  EXPECT_FALSE(peer_context2.complete);

  NotifyRequestComplete(id2, strlen(kTestPageContents2));
  EXPECT_EQ(kTestPageContents2, peer_context2.data);
  EXPECT_TRUE(peer_context2.complete);

  EXPECT_EQ(0u, queued_messages());
}

// Tests that the cancel method prevents other messages from being received.
TEST_F(ResourceDispatcherTest, Cancel) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  int request_id = StartAsync(std::move(request), NULL, &peer_context);

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  // Cancel the request.
  dispatcher()->Cancel(request_id);
  ConsumeCancelRequest(id);

  // Any future messages related to the request should be ignored.
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.received_response);
  EXPECT_FALSE(peer_context.complete);
}

// Tests that calling cancel during a callback works as expected.
TEST_F(ResourceDispatcherTest, CancelDuringCallback) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  StartAsync(std::move(request), NULL, &peer_context);
  peer_context.cancel_on_receive_response = true;

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id);
  EXPECT_TRUE(peer_context.received_response);
  // Request should have been cancelled.
  ConsumeCancelRequest(id);

  // Any future messages related to the request should be ignored.
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
}

class TestResourceDispatcherDelegate : public ResourceDispatcherDelegate {
 public:
  TestResourceDispatcherDelegate() {}
  ~TestResourceDispatcherDelegate() override {}

  std::unique_ptr<RequestPeer> OnRequestComplete(
      std::unique_ptr<RequestPeer> current_peer,
      ResourceType resource_type,
      int error_code) override {
    return current_peer;
  }

  std::unique_ptr<RequestPeer> OnReceivedResponse(
      std::unique_ptr<RequestPeer> current_peer,
      const std::string& mime_type,
      const GURL& url) override {
    return base::MakeUnique<WrapperPeer>(std::move(current_peer));
  }

  class WrapperPeer : public RequestPeer {
   public:
    explicit WrapperPeer(std::unique_ptr<RequestPeer> original_peer)
        : original_peer_(std::move(original_peer)) {}

    void OnUploadProgress(uint64_t position, uint64_t size) override {}

    bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                            const ResourceResponseInfo& info) override {
      return false;
    }

    void OnReceivedResponse(const ResourceResponseInfo& info) override {
      response_info_ = info;
    }

    void OnDownloadedData(int len, int encoded_data_length) override {}

    void OnReceivedData(std::unique_ptr<ReceivedData> data) override {
      data_.append(data->payload(), data->length());
    }
    void OnTransferSizeUpdated(int transfer_size_diff) override {}

    void OnCompletedRequest(int error_code,
                            bool stale_copy_in_cache,
                            const base::TimeTicks& completion_time,
                            int64_t total_transfer_size,
                            int64_t encoded_body_size,
                            int64_t decoded_body_size) override {
      original_peer_->OnReceivedResponse(response_info_);
      if (!data_.empty()) {
        original_peer_->OnReceivedData(
            base::MakeUnique<FixedReceivedData>(data_.data(), data_.size()));
      }
      original_peer_->OnCompletedRequest(error_code, stale_copy_in_cache,
                                         completion_time, total_transfer_size,
                                         encoded_body_size, decoded_body_size);
    }

   private:
    std::unique_ptr<RequestPeer> original_peer_;
    ResourceResponseInfo response_info_;
    std::string data_;

    DISALLOW_COPY_AND_ASSIGN(WrapperPeer);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherDelegate);
};

TEST_F(ResourceDispatcherTest, DelegateTest) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  StartAsync(std::move(request), nullptr, &peer_context);

  // Set the delegate that inserts a new peer in OnReceivedResponse.
  TestResourceDispatcherDelegate delegate;
  dispatcher()->set_delegate(&delegate);

  // Run a simple round-trip.
  const size_t kFirstReceiveSize = 2;
  ASSERT_LT(kFirstReceiveSize, strlen(kTestPageContents));

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  // The wrapper eats all messages until RequestComplete message is sent.
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, std::string(kTestPageContents, kFirstReceiveSize));
  ConsumeDataReceived_ACK(id);
  NotifyDataReceived(id, kTestPageContents + kFirstReceiveSize);
  ConsumeDataReceived_ACK(id);

  EXPECT_FALSE(peer_context.received_response);
  EXPECT_EQ(0u, queued_messages());

  // This lets the wrapper peer pass all the messages to the original
  // peer at once.
  NotifyRequestComplete(id, strlen(kTestPageContents));

  EXPECT_TRUE(peer_context.received_response);
  EXPECT_EQ(kTestPageContents, peer_context.data);
  EXPECT_TRUE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());
}

TEST_F(ResourceDispatcherTest, CancelDuringCallbackWithWrapperPeer) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  StartAsync(std::move(request), nullptr, &peer_context);
  peer_context.cancel_on_receive_response = true;

  // Set the delegate that inserts a new peer in OnReceivedResponse.
  TestResourceDispatcherDelegate delegate;
  dispatcher()->set_delegate(&delegate);

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  // The wrapper eats all messages until RequestComplete message is sent.
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  ConsumeDataReceived_ACK(id);

  EXPECT_FALSE(peer_context.received_response);
  EXPECT_EQ(0u, queued_messages());

  // This lets the wrapper peer pass all the messages to the original
  // peer at once, but the original peer cancels right after it receives
  // the response. (This will remove pending request info from
  // ResourceDispatcher while the wrapper peer is still running
  // OnCompletedRequest, but it should not lead to crashes.)
  NotifyRequestComplete(id, strlen(kTestPageContents));

  EXPECT_TRUE(peer_context.received_response);
  // Request should have been cancelled with no additional messages.
  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer_context.cancelled);

  // Any future messages related to the request should be ignored.
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
}

// Checks that redirects work as expected.
TEST_F(ResourceDispatcherTest, Redirect) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  StartAsync(std::move(request), NULL, &peer_context);

  int id = ConsumeRequestResource();

  NotifyReceivedRedirect(id);
  ConsumeFollowRedirect(id);
  EXPECT_EQ(1, peer_context.seen_redirects);

  NotifyReceivedRedirect(id);
  ConsumeFollowRedirect(id);
  EXPECT_EQ(2, peer_context.seen_redirects);

  NotifyReceivedResponse(id);
  EXPECT_TRUE(peer_context.received_response);

  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  ConsumeDataReceived_ACK(id);

  NotifyRequestComplete(id, strlen(kTestPageContents));
  EXPECT_EQ(kTestPageContents, peer_context.data);
  EXPECT_TRUE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ(2, peer_context.seen_redirects);
}

// Tests that that cancelling during a redirect method prevents other messages
// from being received.
TEST_F(ResourceDispatcherTest, CancelDuringRedirect) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  StartAsync(std::move(request), NULL, &peer_context);
  peer_context.follow_redirects = false;

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  // Redirect the request, which triggers a cancellation.
  NotifyReceivedRedirect(id);
  ConsumeCancelRequest(id);
  EXPECT_EQ(1, peer_context.seen_redirects);
  EXPECT_EQ(0u, queued_messages());

  // Any future messages related to the request should be ignored.  In practice,
  // only the NotifyRequestComplete should be received after this point.
  NotifyReceivedRedirect(id);
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(1, peer_context.seen_redirects);
}

// Checks that deferring a request delays messages until it's resumed.
TEST_F(ResourceDispatcherTest, Defer) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  int request_id = StartAsync(std::move(request), NULL, &peer_context);

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  dispatcher()->SetDefersLoading(request_id, true);
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  // None of the messages should have been processed yet, so no queued messages
  // to the browser process, and no data received by the peer.
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(0, peer_context.seen_redirects);

  // Resuming the request should asynchronously unleash the deferred messages.
  dispatcher()->SetDefersLoading(request_id, false);
  base::RunLoop().RunUntilIdle();

  ConsumeDataReceived_ACK(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer_context.received_response);
  EXPECT_EQ(kTestPageContents, peer_context.data);
  EXPECT_TRUE(peer_context.complete);
}

// Checks that deferring a request during a redirect delays messages until it's
// resumed.
TEST_F(ResourceDispatcherTest, DeferOnRedirect) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  int request_id = StartAsync(std::move(request), NULL, &peer_context);
  peer_context.defer_on_redirect = true;

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  // The request should be deferred during the redirect, including the message
  // to follow the redirect.
  NotifyReceivedRedirect(id);
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);
  NotifyRequestComplete(id, strlen(kTestPageContents));

  // None of the messages should have been processed yet, so no queued messages
  // to the browser process, and no data received by the peer.
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(1, peer_context.seen_redirects);

  // Resuming the request should asynchronously unleash the deferred messages.
  dispatcher()->SetDefersLoading(request_id, false);
  base::RunLoop().RunUntilIdle();

  ConsumeFollowRedirect(id);
  ConsumeDataReceived_ACK(id);

  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer_context.received_response);
  EXPECT_EQ(kTestPageContents, peer_context.data);
  EXPECT_TRUE(peer_context.complete);
  EXPECT_EQ(1, peer_context.seen_redirects);
}

// Checks that a deferred request that's cancelled doesn't receive any messages.
TEST_F(ResourceDispatcherTest, CancelDeferredRequest) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  int request_id = StartAsync(std::move(request), NULL, &peer_context);

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  dispatcher()->SetDefersLoading(request_id, true);
  NotifyReceivedRedirect(id);
  dispatcher()->Cancel(request_id);
  ConsumeCancelRequest(id);

  NotifyRequestComplete(id, 0);
  base::RunLoop().RunUntilIdle();

  // None of the messages should have been processed.
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(0, peer_context.seen_redirects);
}

// Checks cancelling a request while flushing deferred requests from
// the FlushDeferredMessages() task.
TEST_F(ResourceDispatcherTest, CancelWhileFlushingDeferredRequests) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  int request_id = StartAsync(std::move(request), NULL, &peer_context);

  // Cancel the request when the data message is handled.
  peer_context.cancel_on_receive_data = true;

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  dispatcher()->SetDefersLoading(request_id, true);
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);

  // None of the messages should have been processed yet.
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());

  dispatcher()->SetDefersLoading(request_id, false);

  // Make sure that the FlushDeferredMessages() task posted from
  // SetDefersLoading() is run. It should dispatch all the deferred
  // messages.
  base::RunLoop().RunUntilIdle();

  // When the deferred DataReceived is dispatched, the handler will
  // cancel the request, but the ACK is sent after the handler
  // returns, so the cancel request ends up before the ACK in the
  // message queue.
  ConsumeCancelRequest(id);
  ConsumeDataReceived_ACK(id);

  // The data was consumed before the handler canceled
  // the request, so the data should have been received.
  EXPECT_EQ(kTestPageContents, peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());
}

// Checks cancelling a request while flushing deferred requests from
// OnMessageReceived().
TEST_F(ResourceDispatcherTest,
       CancelWhileFlushingDeferredRequestsFromOnMessageReceived) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
  TestRequestPeer::Context peer_context;
  int request_id = StartAsync(std::move(request), NULL, &peer_context);

  // Cancel the request when the data message is handled.
  peer_context.cancel_on_receive_data = true;

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  dispatcher()->SetDefersLoading(request_id, true);
  NotifyReceivedResponse(id);
  NotifySetDataBuffer(id, strlen(kTestPageContents));
  NotifyDataReceived(id, kTestPageContents);

  // None of the messages should have been processed yet.
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());

  dispatcher()->SetDefersLoading(request_id, false);

  // SetDefersLoading() posts a task to run FlushDeferredMessages() to dispatch
  // the deferred messages. Since the message loop hasn't been run yet the
  // task hasn't been run either and no IPC-messages should have been
  // dispatched.
  EXPECT_EQ("", peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());

  // Calling NotifyRequestComplete() here, before the task from
  // SetDefersLoading() has been run, triggers the flush in
  // OnMessageReceived().
  NotifyRequestComplete(id, strlen(kTestPageContents));

  // When the deferred DataReceived is dispatched, the handler will
  // cancel the request, but the ACK is sent after the handler
  // returns, so the cancel request ends up before the ACK in the
  // message queue.
  ConsumeCancelRequest(id);
  ConsumeDataReceived_ACK(id);

  // The data was consumed before the handler canceled
  // the request, so the data should have been received.
  EXPECT_EQ(kTestPageContents, peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());

  // Make sure that the FlushDeferredMessages() task posted from
  // SetDefersLoading() is run. The messages should already have been
  // flushed above, so it should be a NOOP.
  base::RunLoop().RunUntilIdle();

  // Check that the task didn't change anything.
  EXPECT_EQ(kTestPageContents, peer_context.data);
  EXPECT_FALSE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());
}

TEST_F(ResourceDispatcherTest, DownloadToFile) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(true));
  TestRequestPeer::Context peer_context;
  int request_id = StartAsync(std::move(request), NULL, &peer_context);
  const int kDownloadedIncrement = 100;
  const int kEncodedIncrement = 50;

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer_context.received_response);

  int expected_total_downloaded_length = 0;
  int expected_total_encoded_data_length = 0;
  for (int i = 0; i < 10; ++i) {
    NotifyDataDownloaded(id, kDownloadedIncrement, kEncodedIncrement);
    expected_total_downloaded_length += kDownloadedIncrement;
    expected_total_encoded_data_length += kEncodedIncrement;
    EXPECT_EQ(expected_total_downloaded_length,
              peer_context.total_downloaded_data_length);
    EXPECT_EQ(expected_total_encoded_data_length,
              peer_context.total_encoded_data_length);
  }

  NotifyRequestComplete(id, strlen(kTestPageContents));
  EXPECT_EQ("", peer_context.data);
  EXPECT_TRUE(peer_context.complete);
  EXPECT_EQ(0u, queued_messages());

  dispatcher()->RemovePendingRequest(request_id);
  ConsumeReleaseDownloadedFile(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_EQ(expected_total_downloaded_length,
            peer_context.total_downloaded_data_length);
  EXPECT_EQ(expected_total_encoded_data_length,
            peer_context.total_encoded_data_length);
}

// Make sure that when a download to file is cancelled, the file is destroyed.
TEST_F(ResourceDispatcherTest, CancelDownloadToFile) {
  std::unique_ptr<ResourceRequest> request(CreateResourceRequest(true));
  TestRequestPeer::Context peer_context;
  int request_id = StartAsync(std::move(request), NULL, &peer_context);

  int id = ConsumeRequestResource();
  EXPECT_EQ(0u, queued_messages());

  NotifyReceivedResponse(id);
  EXPECT_EQ(0u, queued_messages());
  EXPECT_TRUE(peer_context.received_response);

  // Cancelling the request deletes the file.
  dispatcher()->Cancel(request_id);
  ConsumeCancelRequest(id);
  ConsumeReleaseDownloadedFile(id);
}

TEST_F(ResourceDispatcherTest, Cookies) {
  // FIXME
}

TEST_F(ResourceDispatcherTest, SerializedPostData) {
  // FIXME
}

class TimeConversionTest : public ResourceDispatcherTest {
 public:
  bool Send(IPC::Message* msg) override {
    delete msg;
    return true;
  }

  void PerformTest(const ResourceResponseHead& response_head) {
    std::unique_ptr<ResourceRequest> request(CreateResourceRequest(false));
    TestRequestPeer::Context peer_context;
    StartAsync(std::move(request), NULL, &peer_context);

    dispatcher()->OnMessageReceived(
        ResourceMsg_ReceivedResponse(0, response_head));
  }

  const ResourceResponseInfo& response_info() const { return response_info_; }

 private:
  ResourceResponseInfo response_info_;
};

// TODO(simonjam): Enable this when 10829031 lands.
TEST_F(TimeConversionTest, DISABLED_ProperlyInitialized) {
  ResourceResponseHead response_head;
  response_head.request_start = base::TimeTicks::FromInternalValue(5);
  response_head.response_start = base::TimeTicks::FromInternalValue(15);
  response_head.load_timing.request_start_time = base::Time::Now();
  response_head.load_timing.request_start =
      base::TimeTicks::FromInternalValue(10);
  response_head.load_timing.connect_timing.connect_start =
      base::TimeTicks::FromInternalValue(13);

  PerformTest(response_head);

  EXPECT_LT(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
  EXPECT_LE(response_head.load_timing.request_start,
            response_info().load_timing.connect_timing.connect_start);
}

TEST_F(TimeConversionTest, PartiallyInitialized) {
  ResourceResponseHead response_head;
  response_head.request_start = base::TimeTicks::FromInternalValue(5);
  response_head.response_start = base::TimeTicks::FromInternalValue(15);

  PerformTest(response_head);

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
}

TEST_F(TimeConversionTest, NotInitialized) {
  ResourceResponseHead response_head;

  PerformTest(response_head);

  EXPECT_EQ(base::TimeTicks(), response_info().load_timing.request_start);
  EXPECT_EQ(base::TimeTicks(),
            response_info().load_timing.connect_timing.dns_start);
}

}  // namespace content

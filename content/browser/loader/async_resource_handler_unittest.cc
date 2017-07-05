// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_resource_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/common/resource_messages.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {

namespace {

std::string GenerateHeader(size_t response_data_size) {
  return base::StringPrintf(
      "HTTP/1.1 200 OK\n"
      "Content-type: text/html\n"
      "Content-Length: %" PRIuS "\n",
      response_data_size);
}

int64_t TotalReceivedBytes(size_t response_data_size) {
  return response_data_size + GenerateHeader(response_data_size).size();
}

std::string GenerateData(size_t response_data_size) {
  return std::string(response_data_size, 'a');
}

class TestProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  TestProtocolHandler(size_t response_data_size)
      : response_data_size_(response_data_size) {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new net::URLRequestTestJob(request, network_delegate,
                                      GenerateHeader(response_data_size_),
                                      GenerateData(response_data_size_), true);
  }

 private:
  size_t response_data_size_;
};

// A subclass of ResourceMessageFilter that records IPC messages that are sent.
class RecordingResourceMessageFilter : public ResourceMessageFilter {
 public:
  RecordingResourceMessageFilter(ResourceContext* resource_context,
                                 net::URLRequestContext* request_context)
      : ResourceMessageFilter(
            0,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            base::Bind(&RecordingResourceMessageFilter::GetContexts,
                       base::Unretained(this)),
            BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)),
        resource_context_(resource_context),
        request_context_(request_context) {
    InitializeForTest();
    set_peer_process_for_testing(base::Process::Current());
  }

  const std::vector<std::unique_ptr<IPC::Message>>& messages() const {
    return messages_;
  }

  // IPC::Sender implementation:
  bool Send(IPC::Message* message) override {
    // Unpickle the base::SharedMemoryHandle to avoid warnings about
    // "MessageAttachmentSet destroyed with unconsumed descriptors".
    if (message->type() == ResourceMsg_SetDataBuffer::ID) {
      ResourceMsg_SetDataBuffer::Param params;
      ResourceMsg_SetDataBuffer::Read(message, &params);
    }
    messages_.push_back(base::WrapUnique(message));
    return true;
  }

 private:
  ~RecordingResourceMessageFilter() override {}

  void GetContexts(ResourceType resource_type,
                   ResourceContext** resource_context,
                   net::URLRequestContext** request_context) {
    *resource_context = resource_context_;
    *request_context = request_context_;
  }

  ResourceContext* const resource_context_;
  net::URLRequestContext* const request_context_;
  std::vector<std::unique_ptr<IPC::Message>> messages_;
};

class AsyncResourceHandlerTest : public ::testing::Test,
                                 public ResourceLoaderDelegate {
 protected:
  AsyncResourceHandlerTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP), context_(true) {}

  void TearDown() override {
    if (filter_)
      filter_->OnChannelClosing();
    // Prevent memory leaks.
    filter_ = nullptr;
    rdh_.Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void CreateRequestWithResponseDataSize(size_t response_data_size) {
    test_job_factory_.SetProtocolHandler(
        "test", base::MakeUnique<TestProtocolHandler>(response_data_size));
    context_.set_job_factory(&test_job_factory_);
    context_.Init();
    std::unique_ptr<net::URLRequest> request =
        context_.CreateRequest(GURL("test:test"), net::DEFAULT_PRIORITY,
                               nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
    resource_context_ = base::MakeUnique<MockResourceContext>(&context_);
    filter_ =
        new RecordingResourceMessageFilter(resource_context_.get(), &context_);
    ResourceRequestInfoImpl* info = new ResourceRequestInfoImpl(
        filter_->requester_info_for_test(),
        0,                                      // route_id
        -1,                                     // frame_tree_node_id
        0,                                      // origin_pid
        0,                                      // request_id
        0,                                      // render_frame_id
        false,                                  // is_main_frame
        false,                                  // parent_is_main_frame
        RESOURCE_TYPE_IMAGE,                    // resource_type
        ui::PAGE_TRANSITION_LINK,               // transition_type
        false,                                  // should_replace_current_entry
        false,                                  // is_download
        false,                                  // is_stream
        false,                                  // allow_download
        false,                                  // has_user_gesture
        false,                                  // enable load timing
        false,                                  // enable upload progress
        false,                                  // do_not_prompt_for_login
        blink::kWebReferrerPolicyDefault,       // referrer_policy
        blink::kWebPageVisibilityStateVisible,  // visibility_state
        resource_context_.get(),                // context
        false,                                  // report_raw_headers
        true,                                   // is_async
        PREVIEWS_OFF,                           // previews_state
        nullptr,                                // body
        false);                                 // initiated_in_secure_context
    info->AssociateWithRequest(request.get());
    std::unique_ptr<AsyncResourceHandler> handler =
        base::MakeUnique<AsyncResourceHandler>(request.get(), &rdh_);
    loader_ = base::MakeUnique<ResourceLoader>(
        std::move(request), std::move(handler), this);
  }

  void StartRequestAndWaitWithResponseDataSize(size_t response_data_size) {
    CreateRequestWithResponseDataSize(response_data_size);
    loader_->StartRequest();
    finish_waiter_.reset(new base::RunLoop);
    finish_waiter_->Run();
  }

  scoped_refptr<RecordingResourceMessageFilter> filter_;

 private:
  // ResourceLoaderDelegate implementation:
  ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      ResourceLoader* loader,
      net::AuthChallengeInfo* auth_info) override {
    return nullptr;
  }

  bool HandleExternalProtocol(ResourceLoader* loader,
                              const GURL& url) override {
    return false;
  }
  void DidStartRequest(ResourceLoader* loader) override {}
  void DidReceiveRedirect(ResourceLoader* loader,
                          const GURL& new_url,
                          ResourceResponse* response) override {}
  void DidReceiveResponse(ResourceLoader* loader,
                          ResourceResponse* response) override {}
  void DidFinishLoading(ResourceLoader* loader) override {
    loader_.reset();
    finish_waiter_->Quit();
  }
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      ResourceLoader* loader) override {
    return nullptr;
  }

  TestBrowserThreadBundle thread_bundle_;
  ResourceDispatcherHostImpl rdh_;
  net::TestURLRequestContext context_;
  net::URLRequestJobFactoryImpl test_job_factory_;
  std::unique_ptr<MockResourceContext> resource_context_;
  std::unique_ptr<ResourceLoader> loader_;
  std::unique_ptr<base::RunLoop> finish_waiter_;
};

TEST_F(AsyncResourceHandlerTest, Construct) {
  CreateRequestWithResponseDataSize(1);
}

TEST_F(AsyncResourceHandlerTest, OneChunkLengths) {
  // Larger than kInlinedLeadingChunkSize and smaller than
  // kMaxAllocationSize.
  constexpr auto kDataSize = 4096;
  StartRequestAndWaitWithResponseDataSize(kDataSize);
  const auto& messages = filter_->messages();
  ASSERT_EQ(4u, messages.size());
  ASSERT_EQ(ResourceMsg_DataReceived::ID, messages[2]->type());
  ResourceMsg_DataReceived::Param params;
  ResourceMsg_DataReceived::Read(messages[2].get(), &params);

  int encoded_data_length = std::get<3>(params);
  EXPECT_EQ(kDataSize, encoded_data_length);

  ASSERT_EQ(ResourceMsg_RequestComplete::ID, messages[3]->type());
  ResourceMsg_RequestComplete::Param completion_params;
  ResourceMsg_RequestComplete::Read(messages[3].get(), &completion_params);
  ResourceRequestCompletionStatus completion_status =
      std::get<1>(completion_params);

  EXPECT_EQ(TotalReceivedBytes(kDataSize),
            completion_status.encoded_data_length);
  EXPECT_EQ(kDataSize, completion_status.encoded_body_length);
}

TEST_F(AsyncResourceHandlerTest, TwoChunksLengths) {
  // Larger than kMaxAllocationSize.
  constexpr auto kDataSize = 64 * 1024;
  StartRequestAndWaitWithResponseDataSize(kDataSize);
  const auto& messages = filter_->messages();
  ASSERT_EQ(5u, messages.size());
  ASSERT_EQ(ResourceMsg_DataReceived::ID, messages[2]->type());
  ResourceMsg_DataReceived::Param params;
  ResourceMsg_DataReceived::Read(messages[2].get(), &params);

  int encoded_data_length = std::get<3>(params);
  EXPECT_EQ(32768, encoded_data_length);

  ASSERT_EQ(ResourceMsg_DataReceived::ID, messages[3]->type());
  ResourceMsg_DataReceived::Read(messages[3].get(), &params);

  encoded_data_length = std::get<3>(params);
  EXPECT_EQ(32768, encoded_data_length);

  ASSERT_EQ(ResourceMsg_RequestComplete::ID, messages[4]->type());
  ResourceMsg_RequestComplete::Param completion_params;
  ResourceMsg_RequestComplete::Read(messages[4].get(), &completion_params);
  ResourceRequestCompletionStatus completion_status =
      std::get<1>(completion_params);
  EXPECT_EQ(TotalReceivedBytes(kDataSize),
            completion_status.encoded_data_length);
  EXPECT_EQ(kDataSize, completion_status.encoded_body_length);
}

}  // namespace

}  // namespace content

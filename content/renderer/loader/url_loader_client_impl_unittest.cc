// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/url_loader_client_impl.h"

#include <vector>
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/test_request_peer.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class URLLoaderClientImplTest : public ::testing::Test,
                                IPC::Sender,
                                mojom::URLLoaderFactory {
 protected:
  URLLoaderClientImplTest()
      : dispatcher_(new ResourceDispatcher(this, message_loop_.task_runner())),
        mojo_binding_(this) {
    mojo_binding_.Bind(mojo::MakeRequest(&url_loader_factory_proxy_));

    request_id_ = dispatcher_->StartAsync(
        base::MakeUnique<ResourceRequest>(), 0, nullptr, url::Origin(),
        TRAFFIC_ANNOTATION_FOR_TESTS, false,
        base::MakeUnique<TestRequestPeer>(dispatcher_.get(),
                                          &request_peer_context_),
        blink::WebURLRequest::LoadingIPCType::kMojo,
        url_loader_factory_proxy_.get(),
        std::vector<std::unique_ptr<URLLoaderThrottle>>(),
        mojo::ScopedDataPipeConsumerHandle());
    request_peer_context_.request_id = request_id_;

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(url_loader_client_);
  }

  void TearDown() override {
    url_loader_client_ = nullptr;
    url_loader_factory_proxy_ = nullptr;
  }

  bool Send(IPC::Message* message) override {
    ADD_FAILURE() << "IPC::Sender::Send should not be called.";
    return false;
  }

  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    url_loader_client_ = std::move(client);
  }

  void Clone(mojom::URLLoaderFactoryRequest request) override { NOTREACHED(); }

  static MojoCreateDataPipeOptions DataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 4096;
    return options;
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<ResourceDispatcher> dispatcher_;
  TestRequestPeer::Context request_peer_context_;
  int request_id_ = 0;
  mojom::URLLoaderClientPtr url_loader_client_;
  mojom::URLLoaderFactoryPtr url_loader_factory_proxy_;
  mojo::Binding<mojom::URLLoaderFactory> mojo_binding_;
};

TEST_F(URLLoaderClientImplTest, OnReceiveResponse) {
  ResourceResponseHead response_head;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);

  EXPECT_FALSE(request_peer_context_.received_response);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
}

TEST_F(URLLoaderClientImplTest, ResponseBody) {
  ResourceResponseHead response_head;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);

  EXPECT_FALSE(request_peer_context_.received_response);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);

  mojo::DataPipe data_pipe(DataPipeOptions());
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("hello", request_peer_context_.data);
}

TEST_F(URLLoaderClientImplTest, OnReceiveRedirect) {
  ResourceResponseHead response_head;
  net::RedirectInfo redirect_info;

  url_loader_client_->OnReceiveRedirect(redirect_info, response_head);

  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, request_peer_context_.seen_redirects);
}

TEST_F(URLLoaderClientImplTest, OnDataDownloaded) {
  ResourceResponseHead response_head;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  url_loader_client_->OnDataDownloaded(8, 13);
  url_loader_client_->OnDataDownloaded(2, 1);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ(0, request_peer_context_.total_downloaded_data_length);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ(10, request_peer_context_.total_downloaded_data_length);
  EXPECT_EQ(14, request_peer_context_.total_encoded_data_length);
}

TEST_F(URLLoaderClientImplTest, OnReceiveCachedMetadata) {
  ResourceResponseHead response_head;
  std::vector<uint8_t> metadata;
  metadata.push_back('a');

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  url_loader_client_->OnReceiveCachedMetadata(metadata);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.cached_metadata.empty());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  ASSERT_EQ(1u, request_peer_context_.cached_metadata.size());
  EXPECT_EQ('a', request_peer_context_.cached_metadata[0]);
}

TEST_F(URLLoaderClientImplTest, OnTransferSizeUpdated) {
  ResourceResponseHead response_head;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnTransferSizeUpdated(4);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ(8, request_peer_context_.total_encoded_data_length);
}

TEST_F(URLLoaderClientImplTest, OnCompleteWithoutResponseBody) {
  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  url_loader_client_->OnComplete(completion_status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
}

TEST_F(URLLoaderClientImplTest, OnCompleteWithResponseBody) {
  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  mojo::DataPipe data_pipe(DataPipeOptions());
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);
  data_pipe.producer_handle.reset();

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ("", request_peer_context_.data);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ("hello", request_peer_context_.data);

  url_loader_client_->OnComplete(completion_status);

  EXPECT_FALSE(request_peer_context_.complete);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_TRUE(request_peer_context_.complete);
}

// Due to the lack of ordering guarantee, it is possible that the response body
// bytes arrives after the completion message. URLLoaderClientImpl should
// restore the order.
TEST_F(URLLoaderClientImplTest, OnCompleteShouldBeTheLastMessage) {
  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  mojo::DataPipe data_pipe(DataPipeOptions());
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(completion_status);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_FALSE(request_peer_context_.complete);

  data_pipe.producer_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_TRUE(request_peer_context_.complete);
}

TEST_F(URLLoaderClientImplTest, CancelOnReceiveResponse) {
  request_peer_context_.cancel_on_receive_response = true;

  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  mojo::DataPipe data_pipe(DataPipeOptions());
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(completion_status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_FALSE(request_peer_context_.cancelled);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_TRUE(request_peer_context_.cancelled);
}

TEST_F(URLLoaderClientImplTest, CancelOnReceiveData) {
  request_peer_context_.cancel_on_receive_data = true;

  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  mojo::DataPipe data_pipe(DataPipeOptions());
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(completion_status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_FALSE(request_peer_context_.cancelled);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_TRUE(request_peer_context_.cancelled);
}

TEST_F(URLLoaderClientImplTest, Defer) {
  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  url_loader_client_->OnComplete(completion_status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
}

TEST_F(URLLoaderClientImplTest, DeferWithResponseBody) {
  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  mojo::DataPipe data_pipe(DataPipeOptions());
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);
  data_pipe.producer_handle.reset();

  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnComplete(completion_status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("hello", request_peer_context_.data);
}

// As "transfer size update" message is handled specially in the implementation,
// we have a separate test.
TEST_F(URLLoaderClientImplTest, DeferWithTransferSizeUpdated) {
  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  mojo::DataPipe data_pipe(DataPipeOptions());
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);
  data_pipe.producer_handle.reset();

  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(completion_status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
}

TEST_F(URLLoaderClientImplTest, SetDeferredDuringFlushingDeferredMessage) {
  request_peer_context_.defer_on_redirect = true;

  net::RedirectInfo redirect_info;
  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  url_loader_client_->OnReceiveRedirect(redirect_info, response_head);
  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);
  mojo::DataPipe data_pipe(DataPipeOptions());
  uint32_t size = 5;
  MojoResult result = data_pipe.producer_handle->WriteData(
      "hello", &size, MOJO_WRITE_DATA_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(5u, size);
  data_pipe.producer_handle.reset();

  url_loader_client_->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(completion_status);

  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_EQ(0, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, request_peer_context_.seen_redirects);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ("", request_peer_context_.data);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);

  dispatcher_->SetDefersLoading(request_id_, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, request_peer_context_.seen_redirects);
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ("hello", request_peer_context_.data);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);
}

TEST_F(URLLoaderClientImplTest,
       SetDeferredDuringFlushingDeferredMessageOnTransferSizeUpdated) {
  request_peer_context_.defer_on_transfer_size_updated = true;

  ResourceResponseHead response_head;
  ResourceRequestCompletionStatus completion_status;

  url_loader_client_->OnReceiveResponse(response_head, base::nullopt, nullptr);

  url_loader_client_->OnTransferSizeUpdated(4);
  url_loader_client_->OnComplete(completion_status);

  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  dispatcher_->SetDefersLoading(request_id_, false);
  EXPECT_FALSE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(0, request_peer_context_.total_encoded_data_length);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_FALSE(request_peer_context_.complete);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);

  dispatcher_->SetDefersLoading(request_id_, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request_peer_context_.received_response);
  EXPECT_TRUE(request_peer_context_.complete);
  EXPECT_EQ(4, request_peer_context_.total_encoded_data_length);
  EXPECT_FALSE(request_peer_context_.cancelled);
}

}  // namespace content

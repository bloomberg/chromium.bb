// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/url_response_body_consumer.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/child/request_info.h"
#include "content/child/resource_dispatcher.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request_completion_status.h"
#include "content/public/child/request_peer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class TestRequestPeer : public RequestPeer {
 public:
  struct Context;
  explicit TestRequestPeer(Context* context) : context_(context) {}

  void OnUploadProgress(uint64_t position, uint64_t size) override {
    ADD_FAILURE() << "OnUploadProgress should not be called.";
  }

  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const ResourceResponseInfo& info) override {
    ADD_FAILURE() << "OnReceivedRedirect should not be called.";
    return false;
  }

  void OnReceivedResponse(const ResourceResponseInfo& info) override {
    ADD_FAILURE() << "OnReceivedResponse should not be called.";
  }

  void OnDownloadedData(int len, int encoded_data_length) override {
    ADD_FAILURE() << "OnDownloadedData should not be called.";
  }

  void OnReceivedData(std::unique_ptr<ReceivedData> data) override {
    EXPECT_FALSE(context_->complete);
    context_->data.append(data->payload(), data->length());
    context_->run_loop_quit_closure.Run();
  }

  void OnCompletedRequest(int error_code,
                          bool was_ignored_by_handler,
                          bool stale_copy_in_cache,
                          const std::string& security_info,
                          const base::TimeTicks& completion_time,
                          int64_t total_transfer_size) override {
    EXPECT_FALSE(context_->complete);
    context_->complete = true;
    context_->error_code = error_code;
    context_->run_loop_quit_closure.Run();
  }

  struct Context {
    // Data received. If downloading to file, remains empty.
    std::string data;
    bool complete = false;
    base::Closure run_loop_quit_closure;
    int error_code = net::OK;
  };

 private:
  Context* context_;

  DISALLOW_COPY_AND_ASSIGN(TestRequestPeer);
};

class URLResponseBodyConsumerTest : public ::testing::Test,
                                    public ::IPC::Sender {
 protected:
  URLResponseBodyConsumerTest()
      : dispatcher_(new ResourceDispatcher(this, message_loop_.task_runner())) {
  }

  ~URLResponseBodyConsumerTest() override {
    dispatcher_.reset();
    base::RunLoop().RunUntilIdle();
  }

  bool Send(IPC::Message* message) override {
    delete message;
    return true;
  }

  std::unique_ptr<RequestInfo> CreateRequestInfo() {
    std::unique_ptr<RequestInfo> request_info(new RequestInfo);
    request_info->method = "GET";
    request_info->url = GURL("http://www.example.com/");
    return request_info;
  }

  MojoCreateDataPipeOptions CreateDataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 1024;
    return options;
  }

  // Returns the request id.
  int SetUpRequestPeer(const RequestInfo& request_info,
                       TestRequestPeer::Context* context) {
    return dispatcher_->StartAsync(
        request_info, nullptr, base::MakeUnique<TestRequestPeer>(context),
        blink::WebURLRequest::LoadingIPCType::ChromeIPC, nullptr);
  }

  void Run(TestRequestPeer::Context* context) {
    base::RunLoop run_loop;
    context->run_loop_quit_closure = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<ResourceDispatcher> dispatcher_;
  static const MojoWriteDataFlags kNone = MOJO_WRITE_DATA_FLAG_NONE;
};

TEST_F(URLResponseBodyConsumerTest, ReceiveData) {
  TestRequestPeer::Context context;
  std::unique_ptr<RequestInfo> request_info(CreateRequestInfo());
  int request_id = SetUpRequestPeer(*request_info, &context);
  mojo::DataPipe data_pipe(CreateDataPipeOptions());

  scoped_refptr<URLResponseBodyConsumer> consumer(new URLResponseBodyConsumer(
      request_id, dispatcher_.get(), std::move(data_pipe.consumer_handle),
      message_loop_.task_runner().get()));

  mojo::ScopedDataPipeProducerHandle writer =
      std::move(data_pipe.producer_handle);
  std::string buffer = "hello";
  uint32_t size = buffer.size();
  MojoResult result =
      mojo::WriteDataRaw(writer.get(), buffer.c_str(), &size, kNone);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(buffer.size(), size);

  Run(&context);

  EXPECT_FALSE(context.complete);
  EXPECT_EQ("hello", context.data);
}

TEST_F(URLResponseBodyConsumerTest, OnCompleteThenClose) {
  TestRequestPeer::Context context;
  std::unique_ptr<RequestInfo> request_info(CreateRequestInfo());
  int request_id = SetUpRequestPeer(*request_info, &context);
  mojo::DataPipe data_pipe(CreateDataPipeOptions());

  scoped_refptr<URLResponseBodyConsumer> consumer(new URLResponseBodyConsumer(
      request_id, dispatcher_.get(), std::move(data_pipe.consumer_handle),
      message_loop_.task_runner().get()));

  consumer->OnComplete(ResourceRequestCompletionStatus());
  mojo::ScopedDataPipeProducerHandle writer =
      std::move(data_pipe.producer_handle);
  std::string buffer = "hello";
  uint32_t size = buffer.size();
  MojoResult result =
      mojo::WriteDataRaw(writer.get(), buffer.c_str(), &size, kNone);
  ASSERT_EQ(MOJO_RESULT_OK, result);
  ASSERT_EQ(buffer.size(), size);

  Run(&context);

  writer.reset();
  EXPECT_FALSE(context.complete);
  EXPECT_EQ("hello", context.data);

  Run(&context);

  EXPECT_TRUE(context.complete);
  EXPECT_EQ("hello", context.data);
}

TEST_F(URLResponseBodyConsumerTest, CloseThenOnComplete) {
  TestRequestPeer::Context context;
  std::unique_ptr<RequestInfo> request_info(CreateRequestInfo());
  int request_id = SetUpRequestPeer(*request_info, &context);
  mojo::DataPipe data_pipe(CreateDataPipeOptions());

  scoped_refptr<URLResponseBodyConsumer> consumer(new URLResponseBodyConsumer(
      request_id, dispatcher_.get(), std::move(data_pipe.consumer_handle),
      message_loop_.task_runner().get()));

  ResourceRequestCompletionStatus status;
  status.error_code = net::ERR_FAILED;
  data_pipe.producer_handle.reset();
  consumer->OnComplete(status);

  Run(&context);

  EXPECT_TRUE(context.complete);
  EXPECT_EQ(net::ERR_FAILED, context.error_code);
  EXPECT_EQ("", context.data);
}

}  // namespace

}  // namespace content

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/navigation_body_loader.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include "third_party/blink/public/web/web_navigation_params.h"

namespace content {

namespace {

class NavigationBodyLoaderTest : public ::testing::Test,
                                 public blink::WebNavigationBodyLoader::Client {
 protected:
  NavigationBodyLoaderTest() {}

  ~NavigationBodyLoaderTest() override { base::RunLoop().RunUntilIdle(); }

  MojoCreateDataPipeOptions CreateDataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 1024;
    return options;
  }

  void CreateBodyLoader() {
    data_pipe_ = std::make_unique<mojo::DataPipe>(CreateDataPipeOptions());
    writer_ = std::move(data_pipe_->producer_handle);
    auto endpoints = network::mojom::URLLoaderClientEndpoints::New();
    endpoints->url_loader_client = mojo::MakeRequest(&client_ptr_);
    blink::WebNavigationParams navigation_params;
    NavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
        CommonNavigationParams(), CommitNavigationParams(), 1 /* request_id */,
        network::ResourceResponseHead(), std::move(endpoints),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        2 /* render_frame_id */, true /* is_main_frame */, &navigation_params);
    loader_ = std::move(navigation_params.body_loader);
  }

  void StartLoading() {
    loader_->StartLoadingBody(this, false /* use_isolated_code_cache */);
    client_ptr_->OnStartLoadingResponseBody(
        std::move(data_pipe_->consumer_handle));
    base::RunLoop().RunUntilIdle();
  }

  void Write(const std::string& buffer) {
    uint32_t size = buffer.size();
    MojoResult result = writer_->WriteData(buffer.c_str(), &size, kNone);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    ASSERT_EQ(buffer.size(), size);
  }

  void Complete(int net_error) {
    client_ptr_->OnComplete(network::URLLoaderCompletionStatus(net_error));
    base::RunLoop().RunUntilIdle();
  }

  void BodyCodeCacheReceived(base::span<const uint8_t>) override {}

  void BodyDataReceived(base::span<const char> data) override {
    ASSERT_TRUE(expecting_data_received_);
    did_receive_data_ = true;
    data_received_ += std::string(data.data(), data.size());
    TakeActions();
    if (run_loop_.running())
      run_loop_.Quit();
  }

  void BodyLoadingFinished(
      base::TimeTicks completion_time,
      int64_t total_encoded_data_length,
      int64_t total_encoded_body_length,
      int64_t total_decoded_body_length,
      bool should_report_corb_blocking,
      const base::Optional<blink::WebURLError>& error) override {
    ASSERT_TRUE(expecting_finished_);
    did_finish_ = true;
    error_ = error;
    TakeActions();
    if (run_loop_.running())
      run_loop_.Quit();
  }

  void TakeActions() {
    if (!buffer_to_write_.empty()) {
      std::string buffer = buffer_to_write_;
      buffer_to_write_ = std::string();
      ExpectDataReceived();
      Write(buffer);
    }
    if (toggle_defers_loading_) {
      toggle_defers_loading_ = false;
      loader_->SetDefersLoading(false);
      loader_->SetDefersLoading(true);
    }
    if (destroy_loader_) {
      destroy_loader_ = false;
      loader_.reset();
    }
  }

  void ExpectDataReceived() {
    expecting_data_received_ = true;
    did_receive_data_ = false;
  }

  void ExpectFinished() {
    expecting_finished_ = true;
    did_finish_ = false;
  }

  std::string TakeDataReceived() {
    std::string data = data_received_;
    data_received_ = std::string();
    return data;
  }

  void Wait() {
    if (expecting_data_received_) {
      if (!did_receive_data_)
        run_loop_.Run();
      ASSERT_TRUE(did_receive_data_);
      expecting_data_received_ = false;
    }
    if (expecting_finished_) {
      if (!did_finish_)
        run_loop_.Run();
      ASSERT_TRUE(did_finish_);
      expecting_finished_ = false;
    }
  }

  base::test::ScopedTaskEnvironment task_environment_;
  static const MojoWriteDataFlags kNone = MOJO_WRITE_DATA_FLAG_NONE;
  network::mojom::URLLoaderClientPtr client_ptr_;
  std::unique_ptr<blink::WebNavigationBodyLoader> loader_;
  std::unique_ptr<mojo::DataPipe> data_pipe_;
  mojo::ScopedDataPipeProducerHandle writer_;

  base::RunLoop run_loop_;
  bool expecting_data_received_ = false;
  bool did_receive_data_ = false;
  bool expecting_finished_ = false;
  bool did_finish_ = false;
  std::string buffer_to_write_;
  bool toggle_defers_loading_ = false;
  bool destroy_loader_ = false;
  std::string data_received_;
  base::Optional<blink::WebURLError> error_;
};

TEST_F(NavigationBodyLoaderTest, SetDefersBeforeStart) {
  CreateBodyLoader();
  loader_->SetDefersLoading(true);
  loader_->SetDefersLoading(false);
  // Should not crash.
}

TEST_F(NavigationBodyLoaderTest, DataReceived) {
  CreateBodyLoader();
  StartLoading();
  ExpectDataReceived();
  Write("hello");
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, DataReceivedFromDataReceived) {
  CreateBodyLoader();
  StartLoading();
  ExpectDataReceived();
  buffer_to_write_ = "world";
  Write("hello");
  Wait();
  EXPECT_EQ("helloworld", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, DestroyFromDataReceived) {
  CreateBodyLoader();
  StartLoading();
  ExpectDataReceived();
  destroy_loader_ = false;
  Write("hello");
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, SetDefersLoadingFromDataReceived) {
  CreateBodyLoader();
  StartLoading();
  ExpectDataReceived();
  toggle_defers_loading_ = true;
  Write("hello");
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, StartDeferred) {
  CreateBodyLoader();
  loader_->SetDefersLoading(true);
  StartLoading();
  Write("hello");
  ExpectDataReceived();
  loader_->SetDefersLoading(false);
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, OnCompleteThenClose) {
  CreateBodyLoader();
  StartLoading();
  Complete(net::ERR_FAILED);
  ExpectFinished();
  writer_.reset();
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, DestroyFromOnCompleteThenClose) {
  CreateBodyLoader();
  StartLoading();
  Complete(net::ERR_FAILED);
  ExpectFinished();
  destroy_loader_ = true;
  writer_.reset();
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, SetDefersLoadingFromOnCompleteThenClose) {
  CreateBodyLoader();
  StartLoading();
  Complete(net::ERR_FAILED);
  ExpectFinished();
  toggle_defers_loading_ = true;
  writer_.reset();
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, CloseThenOnComplete) {
  CreateBodyLoader();
  StartLoading();
  writer_.reset();
  ExpectFinished();
  Complete(net::ERR_FAILED);
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, DestroyFromCloseThenOnComplete) {
  CreateBodyLoader();
  StartLoading();
  writer_.reset();
  ExpectFinished();
  destroy_loader_ = true;
  Complete(net::ERR_FAILED);
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, SetDefersLoadingFromCloseThenOnComplete) {
  CreateBodyLoader();
  StartLoading();
  writer_.reset();
  ExpectFinished();
  toggle_defers_loading_ = true;
  Complete(net::ERR_FAILED);
  Wait();
  EXPECT_TRUE(error_.has_value());
}

}  // namespace

}  // namespace content

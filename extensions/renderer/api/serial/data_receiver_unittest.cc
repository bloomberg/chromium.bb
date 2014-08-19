// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "device/serial/data_source_sender.h"
#include "device/serial/data_stream.mojom.h"
#include "extensions/renderer/api_test_base.h"
#include "grit/extensions_renderer_resources.h"

namespace extensions {

// Runs tests defined in extensions/test/data/data_receiver_unittest.js
class DataReceiverTest : public ApiTestBase {
 public:
  DataReceiverTest() {}

  virtual void SetUp() OVERRIDE {
    ApiTestBase::SetUp();
    env()->RegisterModule("async_waiter", IDR_ASYNC_WAITER_JS);
    env()->RegisterModule("data_receiver", IDR_DATA_RECEIVER_JS);
    env()->RegisterModule("device/serial/data_stream.mojom",
                          IDR_DATA_STREAM_MOJOM_JS);
    service_provider()->AddService(base::Bind(
        &DataReceiverTest::CreateDataSource, base::Unretained(this)));
  }

  virtual void TearDown() OVERRIDE {
    if (sender_) {
      sender_->ShutDown();
      sender_ = NULL;
    }
    ApiTestBase::TearDown();
  }

  std::queue<int32_t> error_to_send_;
  std::queue<std::string> data_to_send_;

 private:
  void CreateDataSource(
      mojo::InterfaceRequest<device::serial::DataSource> request) {
    sender_ = mojo::WeakBindToRequest(
        new device::DataSourceSender(
            base::Bind(&DataReceiverTest::ReadyToSend, base::Unretained(this)),
            base::Bind(base::DoNothing)),
        &request);
  }

  void ReadyToSend(scoped_ptr<device::WritableBuffer> buffer) {
    if (data_to_send_.empty() && error_to_send_.empty())
      return;

    std::string data;
    int32_t error = 0;
    if (!data_to_send_.empty()) {
      data = data_to_send_.front();
      data_to_send_.pop();
    }
    if (!error_to_send_.empty()) {
      error = error_to_send_.front();
      error_to_send_.pop();
    }
    if (error == 2) {
      sender_->ShutDown();
      sender_ = NULL;
      return;
    }
    DCHECK(buffer->GetSize() >= static_cast<uint32_t>(data.size()));
    memcpy(buffer->GetData(), data.c_str(), data.size());
    if (error)
      buffer->DoneWithError(data.size(), error);
    else
      buffer->Done(data.size());
  }

  scoped_refptr<device::DataSourceSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(DataReceiverTest);
};

TEST_F(DataReceiverTest, Receive) {
  data_to_send_.push("a");
  RunTest("data_receiver_unittest.js", "testReceive");
}

TEST_F(DataReceiverTest, ReceiveError) {
  error_to_send_.push(1);
  RunTest("data_receiver_unittest.js", "testReceiveError");
}

TEST_F(DataReceiverTest, ReceiveDataAndError) {
  data_to_send_.push("a");
  data_to_send_.push("b");
  error_to_send_.push(1);
  RunTest("data_receiver_unittest.js", "testReceiveDataAndError");
}

TEST_F(DataReceiverTest, ReceiveErrorThenData) {
  data_to_send_.push("");
  data_to_send_.push("a");
  error_to_send_.push(1);
  RunTest("data_receiver_unittest.js", "testReceiveErrorThenData");
}

TEST_F(DataReceiverTest, SourceShutdown) {
  error_to_send_.push(2);
  RunTest("data_receiver_unittest.js", "testSourceShutdown");
}

}  // namespace extensions

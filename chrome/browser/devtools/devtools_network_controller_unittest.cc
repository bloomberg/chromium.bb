// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_controller.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "chrome/browser/devtools/devtools_network_interceptor.h"
#include "chrome/browser/devtools/devtools_network_transaction.h"
#include "chrome/browser/devtools/devtools_network_upload_data_stream.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/http/http_transaction_test_util.h"
#include "net/log/net_log_with_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace test {

using net::kSimpleGET_Transaction;
using net::MockHttpRequest;
using net::MockNetworkLayer;
using net::MockTransaction;
using net::TEST_MODE_SYNC_NET_START;

const char kClientId[] = "42";
const char kAnotherClientId[] = "24";
const char kUploadData[] = "upload_data";
int64_t kUploadIdentifier = 17;

class TestCallback {
 public:
  TestCallback() : run_count_(0), value_(0) {}
  void Run(int value) {
    run_count_++;
    value_ = value;
  }
  int run_count() { return run_count_; }
  int value() { return value_; }

 private:
  int run_count_;
  int value_;
};

class DevToolsNetworkControllerHelper {
 public:
  DevToolsNetworkControllerHelper() :
      completion_callback_(
          base::Bind(&TestCallback::Run, base::Unretained(&callback_))),
      mock_transaction_(kSimpleGET_Transaction),
      buffer_(new net::IOBuffer(64)) {
    mock_transaction_.test_mode = TEST_MODE_SYNC_NET_START;
    mock_transaction_.url = "http://dot.com";
    mock_transaction_.request_headers =
        "X-DevTools-Emulate-Network-Conditions-Client-Id: 42\r\n";
    AddMockTransaction(&mock_transaction_);

    std::unique_ptr<net::HttpTransaction> network_transaction;
    network_layer_.CreateTransaction(
        net::DEFAULT_PRIORITY, &network_transaction);
    transaction_.reset(new DevToolsNetworkTransaction(
        &controller_, std::move(network_transaction)));
  }

  void SetNetworkState(bool offline, double download, double upload) {
    std::unique_ptr<DevToolsNetworkConditions> conditions(
        new DevToolsNetworkConditions(offline, 0, download, upload));
    controller_.SetNetworkState(kClientId, std::move(conditions));
  }

  void SetNetworkState(const std::string& id, bool offline) {
    std::unique_ptr<DevToolsNetworkConditions> conditions(
        new DevToolsNetworkConditions(offline));
    controller_.SetNetworkState(id, std::move(conditions));
  }

  int Start(bool with_upload) {
    request_.reset(new MockHttpRequest(mock_transaction_));

    if (with_upload) {
      upload_data_stream_.reset(
          new net::ChunkedUploadDataStream(kUploadIdentifier));
      upload_data_stream_->AppendData(
          kUploadData, arraysize(kUploadData), true);
      request_->upload_data_stream = upload_data_stream_.get();
    }

    int rv = transaction_->Start(request_.get(), completion_callback_,
                                 net::NetLogWithSource());
    EXPECT_EQ(with_upload, !!transaction_->custom_upload_data_stream_);
    return rv;
  }

  int Read() {
    return transaction_->Read(buffer_.get(), 64, completion_callback_);
  }

  bool ShouldFail() {
    if (transaction_->interceptor_)
      return transaction_->interceptor_->IsOffline();
    DevToolsNetworkInterceptor* interceptor =
        controller_.GetInterceptor(kClientId);
    EXPECT_TRUE(!!interceptor);
    return interceptor->IsOffline();
  }

  bool HasStarted() {
    return !!transaction_->request_;
  }

  bool HasFailed() {
    return transaction_->failed_;
  }

  void CancelTransaction() {
    transaction_.reset();
  }

  int ReadUploadData() {
    EXPECT_EQ(net::OK, transaction_->custom_upload_data_stream_->Init(
                           completion_callback_, net::NetLogWithSource()));
    return transaction_->custom_upload_data_stream_->Read(
        buffer_.get(), 64, completion_callback_);
  }

  ~DevToolsNetworkControllerHelper() {
    RemoveMockTransaction(&mock_transaction_);
  }

  TestCallback* callback() { return &callback_; }
  DevToolsNetworkController* controller() { return &controller_; }
  DevToolsNetworkTransaction* transaction() { return transaction_.get(); }

 private:
  base::MessageLoop message_loop_;
  MockNetworkLayer network_layer_;
  TestCallback callback_;
  net::CompletionCallback completion_callback_;
  MockTransaction mock_transaction_;
  DevToolsNetworkController controller_;
  std::unique_ptr<DevToolsNetworkTransaction> transaction_;
  scoped_refptr<net::IOBuffer> buffer_;
  std::unique_ptr<net::ChunkedUploadDataStream> upload_data_stream_;
  std::unique_ptr<MockHttpRequest> request_;
};

TEST(DevToolsNetworkControllerTest, SingleDisableEnable) {
  DevToolsNetworkControllerHelper helper;
  helper.SetNetworkState(false, 0, 0);
  helper.Start(false);

  EXPECT_FALSE(helper.ShouldFail());
  helper.SetNetworkState(true, 0, 0);
  EXPECT_TRUE(helper.ShouldFail());
  helper.SetNetworkState(false, 0, 0);
  EXPECT_FALSE(helper.ShouldFail());

  base::RunLoop().RunUntilIdle();
}

TEST(DevToolsNetworkControllerTest, InterceptorIsolation) {
  DevToolsNetworkControllerHelper helper;
  helper.SetNetworkState(false, 0, 0);
  helper.Start(false);

  EXPECT_FALSE(helper.ShouldFail());
  helper.SetNetworkState(kAnotherClientId, true);
  EXPECT_FALSE(helper.ShouldFail());
  helper.SetNetworkState(true, 0, 0);
  EXPECT_TRUE(helper.ShouldFail());

  helper.SetNetworkState(kAnotherClientId, false);
  helper.SetNetworkState(false, 0, 0);
  base::RunLoop().RunUntilIdle();
}

TEST(DevToolsNetworkControllerTest, FailOnStart) {
  DevToolsNetworkControllerHelper helper;
  helper.SetNetworkState(true, 0, 0);

  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::ERR_INTERNET_DISCONNECTED);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(helper.callback()->run_count(), 0);
}

TEST(DevToolsNetworkControllerTest, FailRunningTransaction) {
  DevToolsNetworkControllerHelper helper;
  helper.SetNetworkState(false, 0, 0);
  TestCallback* callback = helper.callback();

  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::OK);

  rv = helper.Read();
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(callback->run_count(), 0);

  helper.SetNetworkState(true, 0, 0);
  EXPECT_EQ(callback->run_count(), 0);

  // Wait until HttpTrancation completes reading and invokes callback.
  // DevToolsNetworkTransaction should report error instead.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(callback->run_count(), 1);
  EXPECT_EQ(callback->value(), net::ERR_INTERNET_DISCONNECTED);

  // Check that transaction is not failed second time.
  helper.SetNetworkState(false, 0, 0);
  helper.SetNetworkState(true, 0, 0);
  EXPECT_EQ(callback->run_count(), 1);
}

TEST(DevToolsNetworkControllerTest, ReadAfterFail) {
  DevToolsNetworkControllerHelper helper;
  helper.SetNetworkState(false, 0, 0);

  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(helper.HasStarted());

  helper.SetNetworkState(true, 0, 0);
  // Not failed yet, as no IO was initiated.
  EXPECT_FALSE(helper.HasFailed());

  rv = helper.Read();
  // Fails on first IO.
  EXPECT_EQ(rv, net::ERR_INTERNET_DISCONNECTED);

  // Check that callback is never invoked.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(helper.callback()->run_count(), 0);
}

TEST(DevToolsNetworkControllerTest, CancelTransaction) {
  DevToolsNetworkControllerHelper helper;
  helper.SetNetworkState(false, 0, 0);

  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(helper.HasStarted());
  helper.CancelTransaction();

  // Should not crash.
  helper.SetNetworkState(true, 0, 0);
  helper.SetNetworkState(false, 0, 0);
  base::RunLoop().RunUntilIdle();
}

TEST(DevToolsNetworkControllerTest, CancelFailedTransaction) {
  DevToolsNetworkControllerHelper helper;
  helper.SetNetworkState(true, 0, 0);

  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::ERR_INTERNET_DISCONNECTED);
  EXPECT_TRUE(helper.HasStarted());
  helper.CancelTransaction();

  // Should not crash.
  helper.SetNetworkState(true, 0, 0);
  helper.SetNetworkState(false, 0, 0);
  base::RunLoop().RunUntilIdle();
}

TEST(DevToolsNetworkControllerTest, UploadDoesNotFail) {
  DevToolsNetworkControllerHelper helper;
  helper.SetNetworkState(true, 0, 0);
  int rv = helper.Start(true);
  EXPECT_EQ(rv, net::ERR_INTERNET_DISCONNECTED);
  rv = helper.ReadUploadData();
  EXPECT_EQ(rv, static_cast<int>(arraysize(kUploadData)));
}

TEST(DevToolsNetworkControllerTest, DownloadOnly) {
  DevToolsNetworkControllerHelper helper;
  TestCallback* callback = helper.callback();

  helper.SetNetworkState(false, 10000000, 0);
  int rv = helper.Start(false);
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(callback->run_count(), 1);
  EXPECT_GE(callback->value(), net::OK);

  rv = helper.Read();
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(callback->run_count(), 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(callback->run_count(), 2);
  EXPECT_GE(callback->value(), net::OK);
}

TEST(DevToolsNetworkControllerTest, UploadOnly) {
  DevToolsNetworkControllerHelper helper;
  TestCallback* callback = helper.callback();

  helper.SetNetworkState(false, 0, 1000000);
  int rv = helper.Start(true);
  EXPECT_EQ(rv, net::OK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(callback->run_count(), 0);

  rv = helper.Read();
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(callback->run_count(), 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(callback->run_count(), 1);
  EXPECT_GE(callback->value(), net::OK);

  rv = helper.ReadUploadData();
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(callback->run_count(), 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(callback->run_count(), 2);
  EXPECT_EQ(callback->value(), static_cast<int>(arraysize(kUploadData)));
}

}  // namespace test

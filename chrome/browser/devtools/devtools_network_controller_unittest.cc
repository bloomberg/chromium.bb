// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/devtools/devtools_network_controller.h"
#include "chrome/browser/devtools/devtools_network_transaction.h"
#include "net/http/http_transaction_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace test {

const char kClientId[] = "42";
const char kAnotherClientId[] = "24";

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
    AddMockTransaction(&mock_transaction_);

    scoped_ptr<net::HttpTransaction> network_transaction;
    network_layer_.CreateTransaction(
        net::DEFAULT_PRIORITY, &network_transaction);
    transaction_.reset(new DevToolsNetworkTransaction(
        &controller_, network_transaction.Pass()));
  }

  net::HttpRequestInfo* GetRequest() {
    if (!request_)
      request_.reset(new MockHttpRequest(mock_transaction_));
    return request_.get();
  }

  void SetNetworkState(const std::string& client_id, bool offline) {
    controller_.SetNetworkStateOnIO(client_id, offline);
  }

  int Start() {
    return transaction_->Start(
        GetRequest(), completion_callback_, net::BoundNetLog());
  }

  int Read() {
    return transaction_->Read(buffer_.get(), 64, completion_callback_);
  }

  ~DevToolsNetworkControllerHelper() {
    RemoveMockTransaction(&mock_transaction_);
  }

  TestCallback* callback() { return &callback_; }
  MockTransaction* mock_transaction() { return &mock_transaction_; }
  DevToolsNetworkController* controller() { return &controller_; }
  DevToolsNetworkTransaction* transaction() { return transaction_.get(); }

 private:
  base::MessageLoop message_loop_;
  MockNetworkLayer network_layer_;
  TestCallback callback_;
  net::CompletionCallback completion_callback_;
  MockTransaction mock_transaction_;
  DevToolsNetworkController controller_;
  scoped_ptr<DevToolsNetworkTransaction> transaction_;
  scoped_refptr<net::IOBuffer> buffer_;
  scoped_ptr<MockHttpRequest> request_;
};

TEST(DevToolsNetworkControllerTest, SingleDisableEnable) {
  DevToolsNetworkControllerHelper helper;
  DevToolsNetworkController* controller = helper.controller();
  net::HttpRequestInfo* request = helper.GetRequest();

  EXPECT_FALSE(controller->ShouldFail(request));
  helper.SetNetworkState(kClientId, true);
  EXPECT_TRUE(controller->ShouldFail(request));
  helper.SetNetworkState(kClientId, false);
  EXPECT_FALSE(controller->ShouldFail(request));
}

TEST(DevToolsNetworkControllerTest, DoubleDisableEnable) {
  DevToolsNetworkControllerHelper helper;
  DevToolsNetworkController* controller = helper.controller();
  net::HttpRequestInfo* request = helper.GetRequest();

  EXPECT_FALSE(controller->ShouldFail(request));
  helper.SetNetworkState(kClientId, true);
  EXPECT_TRUE(controller->ShouldFail(request));
  helper.SetNetworkState(kAnotherClientId, true);
  EXPECT_TRUE(controller->ShouldFail(request));
  helper.SetNetworkState(kClientId, false);
  EXPECT_TRUE(controller->ShouldFail(request));
  helper.SetNetworkState(kAnotherClientId, false);
  EXPECT_FALSE(controller->ShouldFail(request));
}

TEST(DevToolsNetworkControllerTest, FailOnStart) {
  DevToolsNetworkControllerHelper helper;
  helper.SetNetworkState(kClientId, true);

  int rv = helper.Start();
  EXPECT_EQ(rv, net::ERR_INTERNET_DISCONNECTED);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(helper.callback()->run_count(), 0);
}

TEST(DevToolsNetworkControllerTest, FailRunningTransaction) {
  DevToolsNetworkControllerHelper helper;
  TestCallback* callback = helper.callback();

  int rv = helper.Start();
  EXPECT_EQ(rv, net::OK);

  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(64));
  rv = helper.Read();
  EXPECT_EQ(rv, net::ERR_IO_PENDING);
  EXPECT_EQ(callback->run_count(), 0);

  helper.SetNetworkState(kClientId, true);
  EXPECT_EQ(callback->run_count(), 1);
  EXPECT_EQ(callback->value(), net::ERR_INTERNET_DISCONNECTED);

  // Wait until HttpTrancation completes reading and invokes callback.
  // DevToolsNetworkTransaction should ignore callback, because it has
  // reported network error already.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(callback->run_count(), 1);

  // Check that transaction in not failed second time.
  helper.SetNetworkState(kClientId, false);
  helper.SetNetworkState(kClientId, true);
  EXPECT_EQ(callback->run_count(), 1);
}

TEST(DevToolsNetworkControllerTest, ReadAfterFail) {
  DevToolsNetworkControllerHelper helper;

  int rv = helper.Start();
  EXPECT_EQ(rv, net::OK);
  EXPECT_TRUE(helper.transaction()->request());

  helper.SetNetworkState(kClientId, true);
  EXPECT_TRUE(helper.transaction()->failed());

  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(64));
  rv = helper.Read();
  EXPECT_EQ(rv, net::ERR_INTERNET_DISCONNECTED);

  // Check that callback is never invoked.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(helper.callback()->run_count(), 0);
}

TEST(DevToolsNetworkControllerTest, AllowsDevToolsRequests) {
  DevToolsNetworkControllerHelper helper;
  helper.mock_transaction()->request_headers =
      "X-DevTools-Request-Initiator: frontend\r\n";
  DevToolsNetworkController* controller = helper.controller();
  net::HttpRequestInfo* request = helper.GetRequest();

  EXPECT_FALSE(controller->ShouldFail(request));
  helper.SetNetworkState(kClientId, true);
  EXPECT_FALSE(controller->ShouldFail(request));
}

}  // namespace test

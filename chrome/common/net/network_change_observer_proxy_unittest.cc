// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/network_change_observer_proxy.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "chrome/common/net/fake_network_change_notifier_thread.h"
#include "chrome/common/net/mock_network_change_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_common_net {

namespace {

// Version of NetworkChangeObserverProxy that records on what thread
// it was deleted.
class DeleteCheckingNetworkChangeObserverProxy
    : public NetworkChangeObserverProxy {
 public:
  // *deleting_message_loop_ must be NULL.  It is set to a non-NULL
  // *value when this object is deleted.
  DeleteCheckingNetworkChangeObserverProxy(
      NetworkChangeNotifierThread* source_thread,
      MessageLoop* target_message_loop,
      MessageLoop** deleting_message_loop)
      : NetworkChangeObserverProxy(source_thread, target_message_loop),
        deleting_message_loop_(deleting_message_loop) {
    CHECK(deleting_message_loop_);
    EXPECT_TRUE(*deleting_message_loop_ == NULL);
  }

 private:
  virtual ~DeleteCheckingNetworkChangeObserverProxy() {
    *deleting_message_loop_ = MessageLoop::current();
  }

  MessageLoop** deleting_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(DeleteCheckingNetworkChangeObserverProxy);
};

class NetworkChangeObserverProxyTest : public testing::Test {
 protected:
  NetworkChangeObserverProxyTest() : proxy_deleting_message_loop_(NULL) {}

  virtual ~NetworkChangeObserverProxyTest() {}

  virtual void SetUp() {
    source_thread_.Start();
    proxy_deleting_message_loop_ = NULL;
    proxy_ = new DeleteCheckingNetworkChangeObserverProxy(
        &source_thread_, &target_message_loop_,
        &proxy_deleting_message_loop_);
  }

  // On TearDown, |proxy_| must be released and both source and target
  // must be pumped.
  virtual void TearDown() {
    EXPECT_TRUE(proxy_ == NULL);
    EXPECT_TRUE(proxy_deleting_message_loop_ != NULL);
    source_thread_.Stop();
  }

  // Pump any events posted on the source thread.
  void PumpSource() {
    source_thread_.Pump();
  }

  // Pump any events posted on the target thread (which is just the
  // main test thread).
  void PumpTarget() {
    target_message_loop_.RunAllPending();
  }

  // Trigger an "IP address changed" event on the source network
  // change notifier on the source thread.
  void NotifyIPAddressChange() {
    source_thread_.NotifyIPAddressChange();
  }

  FakeNetworkChangeNotifierThread source_thread_;

  MessageLoop target_message_loop_;
  MockNetworkChangeObserver target_observer_;

  MessageLoop* proxy_deleting_message_loop_;
  scoped_refptr<NetworkChangeObserverProxy> proxy_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeObserverProxyTest);
};

// Make sure nothing blows up if we don't attach the proxy.
TEST_F(NetworkChangeObserverProxyTest, DoNothing) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  proxy_ = NULL;
  // No need to pump.

  EXPECT_EQ(proxy_deleting_message_loop_, &target_message_loop_);
}

TEST_F(NetworkChangeObserverProxyTest, AttachDetach) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  proxy_->Attach(&target_observer_);
  proxy_->Detach();
  proxy_ = NULL;
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_thread_.GetMessageLoop());
}

TEST_F(NetworkChangeObserverProxyTest, AttachDetachReleaseAfterPump) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  proxy_->Attach(&target_observer_);
  proxy_->Detach();
  PumpSource();
  proxy_ = NULL;

  EXPECT_EQ(proxy_deleting_message_loop_, &target_message_loop_);
}

TEST_F(NetworkChangeObserverProxyTest, Basic) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(1);

  proxy_->Attach(&target_observer_);
  NotifyIPAddressChange();
  PumpSource();
  PumpTarget();
  proxy_->Detach();
  proxy_ = NULL;
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_thread_.GetMessageLoop());
}

TEST_F(NetworkChangeObserverProxyTest, Multiple) {
  const int kTimes = 5;
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(kTimes);

  proxy_->Attach(&target_observer_);
  for (int i = 0; i < kTimes; ++i) {
    NotifyIPAddressChange();
  }
  PumpSource();
  PumpTarget();
  proxy_->Detach();
  proxy_ = NULL;
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_thread_.GetMessageLoop());
}

TEST_F(NetworkChangeObserverProxyTest, MultipleAttachDetach) {
  const int kTimes = 5;
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(kTimes);

  for (int i = 0; i < kTimes; ++i) {
    proxy_->Attach(&target_observer_);
    NotifyIPAddressChange();
    PumpSource();
    PumpTarget();
    proxy_->Detach();
  }
  proxy_ = NULL;
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_thread_.GetMessageLoop());
}

TEST_F(NetworkChangeObserverProxyTest, IgnoresEventPostedAfterDetach) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  proxy_->Attach(&target_observer_);
  NotifyIPAddressChange();
  proxy_->Detach();
  proxy_ = NULL;
  PumpSource();
  PumpTarget();

  EXPECT_EQ(proxy_deleting_message_loop_, &target_message_loop_);
}

TEST_F(NetworkChangeObserverProxyTest, IgnoresEventPostedBeforeDetach) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  proxy_->Attach(&target_observer_);
  NotifyIPAddressChange();
  PumpSource();
  proxy_->Detach();
  proxy_ = NULL;
  PumpTarget();
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_thread_.GetMessageLoop());
}

TEST_F(NetworkChangeObserverProxyTest, IgnoresEventAfterDetach) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  proxy_->Attach(&target_observer_);
  proxy_->Detach();
  proxy_ = NULL;
  NotifyIPAddressChange();
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_thread_.GetMessageLoop());
}

TEST_F(NetworkChangeObserverProxyTest, IgnoresEventBeforeAttach) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  NotifyIPAddressChange();
  PumpSource();
  proxy_->Attach(&target_observer_);
  proxy_->Detach();
  proxy_ = NULL;
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_thread_.GetMessageLoop());
}

}  // namespace

}  // namespace chrome_common_net

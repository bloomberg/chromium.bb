// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/net/network_change_observer_proxy.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/sync/net/mock_network_change_observer.h"
#include "chrome/browser/sync/net/thread_blocker.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// We manage the lifetime of net::MockNetworkChangeNotifier ourselves.
template <>
struct RunnableMethodTraits<net::MockNetworkChangeNotifier> {
  void RetainCallee(net::MockNetworkChangeNotifier*) {}
  void ReleaseCallee(net::MockNetworkChangeNotifier*) {}
};

namespace browser_sync {

namespace {

// Version of NetworkChangeObserverProxy that records on what thread
// it was deleted.
class DeleteCheckingNetworkChangeObserverProxy
    : public NetworkChangeObserverProxy {
 public:
  // *deleting_message_loop_ must be NULL.  It is set to a non-NULL
  // *value when this object is deleted.
  DeleteCheckingNetworkChangeObserverProxy(
      MessageLoop* source_message_loop,
      net::NetworkChangeNotifier* source_network_change_notifier,
      MessageLoop* target_message_loop,
      MessageLoop** deleting_message_loop)
      : NetworkChangeObserverProxy(source_message_loop,
                                   source_network_change_notifier,
                                   target_message_loop),
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
  NetworkChangeObserverProxyTest()
      : source_thread_("Source Thread"),
        source_message_loop_(NULL),
        proxy_deleting_message_loop_(NULL) {}

  virtual ~NetworkChangeObserverProxyTest() {
    CHECK(!source_thread_blocker_.get());
    CHECK(!source_message_loop_);
  }

  virtual void SetUp() {
    CHECK(source_thread_.Start());
    source_message_loop_ = source_thread_.message_loop();
    source_thread_blocker_.reset(new ThreadBlocker(&source_thread_));
    source_thread_blocker_->Block();

    proxy_deleting_message_loop_ = NULL;
    proxy_ = new DeleteCheckingNetworkChangeObserverProxy(
        source_message_loop_, &source_network_change_notifier_,
        &target_message_loop_, &proxy_deleting_message_loop_);
  }

  // On TearDown, |proxy_| must be released and both source and target
  // must be pumped.
  virtual void TearDown() {
    EXPECT_TRUE(proxy_ == NULL);
    EXPECT_TRUE(proxy_deleting_message_loop_ != NULL);
    source_thread_blocker_->Unblock();
    source_thread_blocker_.reset();
    source_message_loop_ = NULL;
    source_thread_.Stop();
  }

  // Pump any events posted on the source thread.
  void PumpSource() {
    source_thread_blocker_->Unblock();
    source_thread_blocker_->Block();
  }

  // Pump any events posted on the target thread (which is just the
  // main test thread).
  void PumpTarget() {
    target_message_loop_.RunAllPending();
  }

  // Trigger an "IP address changed" event on the source network
  // change notifier on the source thread.
  void OnIPAddressChanged() {
    source_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            &source_network_change_notifier_,
            &net::MockNetworkChangeNotifier::NotifyIPAddressChange));
  }

  // This lives on |source_thread_| but is created/destroyed in the
  // main thread.  As long as we make sure to not modify it from the
  // main thread while |source_thread_| is running, we shouldn't need
  // any special synchronization.
  net::MockNetworkChangeNotifier source_network_change_notifier_;
  base::Thread source_thread_;
  MessageLoop* source_message_loop_;
  scoped_ptr<ThreadBlocker> source_thread_blocker_;

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

  EXPECT_EQ(proxy_deleting_message_loop_, source_message_loop_);
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
  OnIPAddressChanged();
  PumpSource();
  PumpTarget();
  proxy_->Detach();
  proxy_ = NULL;
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_message_loop_);
}

TEST_F(NetworkChangeObserverProxyTest, Multiple) {
  const int kTimes = 5;
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(kTimes);

  proxy_->Attach(&target_observer_);
  for (int i = 0; i < kTimes; ++i) {
    OnIPAddressChanged();
  }
  PumpSource();
  PumpTarget();
  proxy_->Detach();
  proxy_ = NULL;
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_message_loop_);
}

TEST_F(NetworkChangeObserverProxyTest, MultipleAttachDetach) {
  const int kTimes = 5;
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(kTimes);

  for (int i = 0; i < kTimes; ++i) {
    proxy_->Attach(&target_observer_);
    OnIPAddressChanged();
    PumpSource();
    PumpTarget();
    proxy_->Detach();
  }
  proxy_ = NULL;
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_message_loop_);
}

TEST_F(NetworkChangeObserverProxyTest, IgnoresEventPostedAfterDetach) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  proxy_->Attach(&target_observer_);
  OnIPAddressChanged();
  proxy_->Detach();
  proxy_ = NULL;
  PumpSource();
  PumpTarget();

  EXPECT_EQ(proxy_deleting_message_loop_, &target_message_loop_);
}

TEST_F(NetworkChangeObserverProxyTest, IgnoresEventPostedBeforeDetach) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  proxy_->Attach(&target_observer_);
  OnIPAddressChanged();
  PumpSource();
  proxy_->Detach();
  proxy_ = NULL;
  PumpTarget();
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_message_loop_);
}

TEST_F(NetworkChangeObserverProxyTest, IgnoresEventAfterDetach) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  proxy_->Attach(&target_observer_);
  proxy_->Detach();
  proxy_ = NULL;
  OnIPAddressChanged();
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_message_loop_);
}

TEST_F(NetworkChangeObserverProxyTest, IgnoresEventBeforeAttach) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  OnIPAddressChanged();
  PumpSource();
  proxy_->Attach(&target_observer_);
  proxy_->Detach();
  proxy_ = NULL;
  PumpSource();

  EXPECT_EQ(proxy_deleting_message_loop_, source_message_loop_);
}

}  // namespace

}  // namespace browser_sync

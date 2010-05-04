// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/net/network_change_notifier_proxy.h"

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

class NetworkChangeNotifierProxyTest : public testing::Test {
 protected:
  NetworkChangeNotifierProxyTest()
      : source_thread_("Source Thread"),
        source_message_loop_(NULL) {}

  virtual ~NetworkChangeNotifierProxyTest() {
    CHECK(!source_thread_blocker_.get());
    CHECK(!source_message_loop_);
  }

  virtual void SetUp() {
    CHECK(source_thread_.Start());
    source_message_loop_ = source_thread_.message_loop();
    source_thread_blocker_.reset(new ThreadBlocker(&source_thread_));
    source_thread_blocker_->Block();

    notifier_proxy_.reset(new NetworkChangeNotifierProxy(
        source_message_loop_, &source_network_change_notifier_));
  }

  virtual void TearDown() {
    // Posts a task to the source thread.
    notifier_proxy_.reset();
    source_thread_blocker_->Unblock();
    source_thread_blocker_.reset();
    source_message_loop_ = NULL;
    source_thread_.Stop();
  }

  // Trigger an "IP address changed" event on the source network
  // change notifier on the source thread and propagate any generated
  // notifications to the target thread.
  void OnIPAddressChanged() {
    source_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            &source_network_change_notifier_,
            &net::MockNetworkChangeNotifier::NotifyIPAddressChange));
    // Pump source thread.
    source_thread_blocker_->Unblock();
    source_thread_blocker_->Block();
    // Pump target thread.
    target_message_loop_.RunAllPending();
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

  scoped_ptr<net::NetworkChangeNotifier> notifier_proxy_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierProxyTest);
};

TEST_F(NetworkChangeNotifierProxyTest, Basic) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(1);

  notifier_proxy_->AddObserver(&target_observer_);
  OnIPAddressChanged();
  notifier_proxy_->RemoveObserver(&target_observer_);
}

TEST_F(NetworkChangeNotifierProxyTest, IgnoresEventAfterRemoveObserver) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  notifier_proxy_->AddObserver(&target_observer_);
  notifier_proxy_->RemoveObserver(&target_observer_);
  OnIPAddressChanged();
}

TEST_F(NetworkChangeNotifierProxyTest, IgnoresEventBeforeRemoveObserver) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  OnIPAddressChanged();
  notifier_proxy_->AddObserver(&target_observer_);
  notifier_proxy_->RemoveObserver(&target_observer_);
}

TEST_F(NetworkChangeNotifierProxyTest, Multiple) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  const int kNumObservers = 5;
  MockNetworkChangeObserver extra_observers[kNumObservers];
  for (int i = 0; i < kNumObservers; ++i) {
    EXPECT_CALL(extra_observers[i], OnIPAddressChanged()).Times(1);
  }

  for (int i = 0; i < kNumObservers; ++i) {
    notifier_proxy_->AddObserver(&extra_observers[i]);
  }
  OnIPAddressChanged();
  for (int i = 0; i < kNumObservers; ++i) {
    notifier_proxy_->RemoveObserver(&extra_observers[i]);
  }
}

}  // namespace

}  // namespace browser_sync

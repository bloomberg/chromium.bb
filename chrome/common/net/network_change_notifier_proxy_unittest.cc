// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/network_change_notifier_proxy.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/fake_network_change_notifier_thread.h"
#include "chrome/common/net/mock_network_change_observer.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_common_net {

namespace {

class NetworkChangeNotifierProxyTest : public testing::Test {
 protected:
  NetworkChangeNotifierProxyTest() {}

  virtual ~NetworkChangeNotifierProxyTest() {}

  virtual void SetUp() {
    source_thread_.Start();
    notifier_proxy_.reset(new NetworkChangeNotifierProxy(&source_thread_));
  }

  virtual void TearDown() {
    // Posts a task to the source thread.
    notifier_proxy_.reset();
    source_thread_.Stop();
  }

  // Trigger an "IP address changed" event on the source network
  // change notifier on the source thread and propagate any generated
  // notifications to the target thread.
  void NotifyIPAddressChange() {
    source_thread_.NotifyIPAddressChange();
    source_thread_.Pump();
    target_message_loop_.RunAllPending();
  }

  FakeNetworkChangeNotifierThread source_thread_;

  MessageLoop target_message_loop_;
  MockNetworkChangeObserver target_observer_;

  scoped_ptr<net::NetworkChangeNotifier> notifier_proxy_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierProxyTest);
};

TEST_F(NetworkChangeNotifierProxyTest, Basic) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(1);

  notifier_proxy_->AddObserver(&target_observer_);
  NotifyIPAddressChange();
  notifier_proxy_->RemoveObserver(&target_observer_);
}

TEST_F(NetworkChangeNotifierProxyTest, IgnoresEventAfterRemoveObserver) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  notifier_proxy_->AddObserver(&target_observer_);
  notifier_proxy_->RemoveObserver(&target_observer_);
  NotifyIPAddressChange();
}

TEST_F(NetworkChangeNotifierProxyTest, IgnoresEventBeforeRemoveObserver) {
  EXPECT_CALL(target_observer_, OnIPAddressChanged()).Times(0);

  NotifyIPAddressChange();
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
  NotifyIPAddressChange();
  for (int i = 0; i < kNumObservers; ++i) {
    notifier_proxy_->RemoveObserver(&extra_observers[i]);
  }
}

}  // namespace

}  // namespace chrome_common_net

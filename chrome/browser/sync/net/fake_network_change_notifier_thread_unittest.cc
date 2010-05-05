// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/net/fake_network_change_notifier_thread.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
class FlagToggler;
}  // namespace browser_sync

// We manage the lifetime of browser_sync::FlagToggler ourselves.
template <>
struct RunnableMethodTraits<browser_sync::FlagToggler> {
  void RetainCallee(browser_sync::FlagToggler*) {}
  void ReleaseCallee(browser_sync::FlagToggler*) {}
};

namespace browser_sync {

// Utility class that toggles a flag every time it receives an IP
// address change notification.
class FlagToggler : public net::NetworkChangeNotifier::Observer {
 public:
  FlagToggler() : flag_(false) {}

  virtual ~FlagToggler() {}

  bool flag() const { return flag_; }

  void ToggleFlag() {
    flag_ = !flag_;
  }

  void Observe(NetworkChangeNotifierThread* thread) {
    thread->GetNetworkChangeNotifier()->AddObserver(this);
  }

  void Unobserve(NetworkChangeNotifierThread* thread) {
    thread->GetNetworkChangeNotifier()->RemoveObserver(this);
  }

  // net::NetworkChangeNotifier::Observer implementation.
  virtual void OnIPAddressChanged() {
    ToggleFlag();
  }

 private:
  bool flag_;

  DISALLOW_COPY_AND_ASSIGN(FlagToggler);
};

namespace {

class FakeNetworkChangeNotifierTest : public testing::Test {
 protected:
  FakeNetworkChangeNotifierTest() {}

  virtual ~FakeNetworkChangeNotifierTest() {}

  virtual void SetUp() {
    thread_.Start();
  }

  virtual void TearDown() {
    thread_.Stop();
  }

  FakeNetworkChangeNotifierThread thread_;
  FlagToggler flag_toggler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeNetworkChangeNotifierTest);
};

TEST_F(FakeNetworkChangeNotifierTest, Pump) {
  thread_.GetMessageLoop()->PostTask(
      FROM_HERE, NewRunnableMethod(&flag_toggler_, &FlagToggler::ToggleFlag));
  EXPECT_FALSE(flag_toggler_.flag());
  thread_.Pump();
  EXPECT_TRUE(flag_toggler_.flag());
}

TEST_F(FakeNetworkChangeNotifierTest, Basic) {
  thread_.GetMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(&flag_toggler_, &FlagToggler::Observe, &thread_));
  thread_.NotifyIPAddressChange();
  thread_.GetMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(&flag_toggler_, &FlagToggler::Unobserve, &thread_));
  EXPECT_FALSE(flag_toggler_.flag());
  thread_.Pump();
  EXPECT_TRUE(flag_toggler_.flag());
}

TEST_F(FakeNetworkChangeNotifierTest, Multiple) {
  FlagToggler observer;
  thread_.GetMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(&flag_toggler_, &FlagToggler::Observe, &thread_));
  thread_.NotifyIPAddressChange();
  thread_.NotifyIPAddressChange();
  thread_.GetMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(&flag_toggler_, &FlagToggler::Unobserve, &thread_));
  EXPECT_FALSE(flag_toggler_.flag());
  thread_.Pump();
  EXPECT_FALSE(flag_toggler_.flag());
}

}  // namespace

}  // namespace browser_sync

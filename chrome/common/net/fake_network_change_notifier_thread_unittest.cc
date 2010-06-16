// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/fake_network_change_notifier_thread.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_common_net {
class FlagToggler;
}  // namespace chrome_common_net

// We manage the lifetime of chrome_common_net::FlagToggler ourselves.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chrome_common_net::FlagToggler);

namespace chrome_common_net {

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

// Utility class that sanity-checks a
// FakeNetworkChangeNotifierThread's member variables right before its
// message loop gets destroyed (used in DestructionRace test).
class FakeNetworkChangeNotifierThreadDestructionObserver
    : public MessageLoop::DestructionObserver {
 public:
  explicit FakeNetworkChangeNotifierThreadDestructionObserver(
      const FakeNetworkChangeNotifierThread& thread)
      : thread_(thread) {}

  virtual ~FakeNetworkChangeNotifierThreadDestructionObserver() {}

  virtual void WillDestroyCurrentMessageLoop() {
    EXPECT_FALSE(thread_.thread_blocker_.get());
    // We can't use
    // FakeNetworkChangeNotifierThread::GetNetworkChangeNotifier() as
    // it would CHECK-fail on the current thread's message loop being
    // NULL.
    EXPECT_TRUE(thread_.network_change_notifier_.get());
    delete this;
  }

 private:
  const FakeNetworkChangeNotifierThread& thread_;

  DISALLOW_COPY_AND_ASSIGN(
      FakeNetworkChangeNotifierThreadDestructionObserver);
};

// Utility function to add the
// FakeNetworkChangeNotifierThreadDestructionObserver from the
// FakeNetworkChangeNotifierThread's thread.
void AddFakeNetworkChangeNotifierThreadDestructionObserver(
    const FakeNetworkChangeNotifierThread* thread) {
  CHECK_EQ(MessageLoop::current(), thread->GetMessageLoop());
  thread->GetMessageLoop()->AddDestructionObserver(
      new FakeNetworkChangeNotifierThreadDestructionObserver(*thread));
}

namespace {

class FakeNetworkChangeNotifierThreadTest : public testing::Test {
 protected:
  FakeNetworkChangeNotifierThreadTest() {}

  virtual ~FakeNetworkChangeNotifierThreadTest() {}

  virtual void SetUp() {
    thread_.Start();
  }

  virtual void TearDown() {
    thread_.Stop();
  }

  FakeNetworkChangeNotifierThread thread_;
  FlagToggler flag_toggler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeNetworkChangeNotifierThreadTest);
};

TEST_F(FakeNetworkChangeNotifierThreadTest, Pump) {
  thread_.GetMessageLoop()->PostTask(
      FROM_HERE, NewRunnableMethod(&flag_toggler_, &FlagToggler::ToggleFlag));
  EXPECT_FALSE(flag_toggler_.flag());
  thread_.Pump();
  EXPECT_TRUE(flag_toggler_.flag());
}

TEST_F(FakeNetworkChangeNotifierThreadTest, Basic) {
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

TEST_F(FakeNetworkChangeNotifierThreadTest, Multiple) {
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

TEST_F(FakeNetworkChangeNotifierThreadTest, DestructionRace) {
  thread_.GetMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(
          &AddFakeNetworkChangeNotifierThreadDestructionObserver,
          &thread_));
}

}  // namespace

}  // namespace chrome_common_net

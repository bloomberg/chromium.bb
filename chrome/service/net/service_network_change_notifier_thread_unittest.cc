// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/net/service_network_change_notifier_thread.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/common/net/thread_blocker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ServiceNetworkChangeNotifierThreadTest : public testing::Test {
 protected:
  ServiceNetworkChangeNotifierThreadTest()
     : io_thread_("ServiceNetworkChangeNotifierThreadTest_IO") {
  }

  virtual ~ServiceNetworkChangeNotifierThreadTest() {}

  virtual void SetUp() {
    // We need to set the message loop type explicitly because
    // IOThread doesn't do it for us and the Linux
    // NetworkChangeNotifier expects it.
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    EXPECT_TRUE(io_thread_.StartWithOptions(options));
    thread_blocker_.reset(new chrome_common_net::ThreadBlocker(&io_thread_));
    thread_blocker_->Block();
  }

  virtual void TearDown() {
    // Nothing should be posted on |io_thread_| at this point.
    thread_blocker_->Unblock();
    thread_blocker_.reset();
    io_thread_.Stop();
  }

  base::Thread io_thread_;
  scoped_ptr<chrome_common_net::ThreadBlocker> thread_blocker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceNetworkChangeNotifierThreadTest);
};

void CheckNonNullNetworkChangeNotifier(
    ServiceNetworkChangeNotifierThread* network_change_notifier_thread) {
  EXPECT_EQ(network_change_notifier_thread->GetMessageLoop(),
            MessageLoop::current());
  EXPECT_TRUE(
      network_change_notifier_thread->GetNetworkChangeNotifier() != NULL);
}

void CheckNullNetworkChangeNotifier(
    ServiceNetworkChangeNotifierThread* network_change_notifier_thread) {
  EXPECT_EQ(network_change_notifier_thread->GetMessageLoop(),
            MessageLoop::current());
  EXPECT_TRUE(
      network_change_notifier_thread->GetNetworkChangeNotifier() == NULL);
}

TEST_F(ServiceNetworkChangeNotifierThreadTest, Basic) {
  scoped_refptr<ServiceNetworkChangeNotifierThread> change_notifier_thread =
      new ServiceNetworkChangeNotifierThread(io_thread_.message_loop(), NULL);
  EXPECT_EQ(io_thread_.message_loop(),
            change_notifier_thread->GetMessageLoop());
  change_notifier_thread->Initialize();
  io_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(&CheckNonNullNetworkChangeNotifier,
                          change_notifier_thread.get()));
  // Pump the thread to make sure the task we just posted is run
  // before this test ends.
  thread_blocker_->Unblock();
  thread_blocker_->Block();
}

TEST_F(ServiceNetworkChangeNotifierThreadTest, Uninitialized) {
  scoped_refptr<ServiceNetworkChangeNotifierThread> change_notifier_thread =
      new ServiceNetworkChangeNotifierThread(io_thread_.message_loop(), NULL);
  EXPECT_EQ(io_thread_.message_loop(),
            change_notifier_thread->GetMessageLoop());
  // We have not called Initialize. Expect the GetNetworkChangeNotifier() call
  // to return NULL.
  io_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableFunction(&CheckNullNetworkChangeNotifier,
                          change_notifier_thread.get()));
  // Pump the thread to make sure the task we just posted is run
  // before this test ends.
  thread_blocker_->Unblock();
  thread_blocker_->Block();
}

}  // namespace

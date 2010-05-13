// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/net/network_change_notifier_io_thread.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/net/thread_blocker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class NetworkChangeNotifierIOThreadTest : public testing::Test {
 protected:
  NetworkChangeNotifierIOThreadTest() {}

  virtual ~NetworkChangeNotifierIOThreadTest() {}

  virtual void SetUp() {
    // We need to set the message loop type explicitly because
    // IOThread doesn't do it for us and the Linux
    // NetworkChangeNotifier expects it.
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    EXPECT_TRUE(io_thread_.StartWithOptions(options));
    thread_blocker_.reset(new chrome_common_net::ThreadBlocker(&io_thread_));
    thread_blocker_->Block();
    network_change_notifier_io_thread_.reset(
        new NetworkChangeNotifierIOThread(&io_thread_));
  }

  virtual void TearDown() {
    network_change_notifier_io_thread_.reset();
    // Nothing should be posted on |io_thread_| at this point;
    // otherwise, it may try to access
    // network_change_notifier_io_thread_, which now NULL
    thread_blocker_->Unblock();
    thread_blocker_.reset();
    io_thread_.Stop();
  }

  IOThread io_thread_;
  scoped_ptr<chrome_common_net::ThreadBlocker> thread_blocker_;
  scoped_ptr<NetworkChangeNotifierIOThread>
      network_change_notifier_io_thread_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierIOThreadTest);
};

void CompareNetworkChangeNotifiers(
    IOThread* io_thread,
    NetworkChangeNotifierIOThread* network_change_notifier_io_thread) {
  EXPECT_EQ(network_change_notifier_io_thread->GetMessageLoop(),
            MessageLoop::current());
  EXPECT_EQ(io_thread->globals()->network_change_notifier.get(),
            network_change_notifier_io_thread->GetNetworkChangeNotifier());
}

TEST_F(NetworkChangeNotifierIOThreadTest, Basic) {
  EXPECT_EQ(io_thread_.message_loop(),
            network_change_notifier_io_thread_->GetMessageLoop());
  ASSERT_TRUE(
      io_thread_.PostTask(ChromeThread::IO,
                          FROM_HERE,
                          NewRunnableFunction(
                              &CompareNetworkChangeNotifiers,
                              &io_thread_,
                              network_change_notifier_io_thread_.get())));
  // Pump the thread to make sure the task we just posted is run
  // before this test ends.
  thread_blocker_->Unblock();
  thread_blocker_->Block();
}

}  // namespace

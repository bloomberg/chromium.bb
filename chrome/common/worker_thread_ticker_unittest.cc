// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/worker_thread_ticker.h"

#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestCallback : public WorkerThreadTicker::Callback {
 public:
  TestCallback() : counter_(0), message_loop_(base::MessageLoop::current()) {}

  virtual void OnTick() OVERRIDE {
    counter_++;

    // Finish the test faster.
    message_loop_->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }

  int counter() const { return counter_; }

 private:
  int counter_;
  base::MessageLoop* message_loop_;
};

class LongCallback : public WorkerThreadTicker::Callback {
 public:
  virtual void OnTick() OVERRIDE {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1500));
  }
};

void RunMessageLoopForAWhile() {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(500));
  base::MessageLoop::current()->Run();
}

}  // namespace

TEST(WorkerThreadTickerTest, Basic) {
  base::MessageLoop loop;

  TestCallback callback;
  WorkerThreadTicker ticker(50);
  EXPECT_FALSE(ticker.IsRunning());
  EXPECT_TRUE(ticker.RegisterTickHandler(&callback));
  EXPECT_TRUE(ticker.UnregisterTickHandler(&callback));
  EXPECT_TRUE(ticker.Start());
  EXPECT_FALSE(ticker.RegisterTickHandler(&callback));
  EXPECT_FALSE(ticker.UnregisterTickHandler(&callback));
  EXPECT_TRUE(ticker.IsRunning());
  EXPECT_FALSE(ticker.Start());  // Can't start when it is running.
  EXPECT_TRUE(ticker.Stop());
  EXPECT_FALSE(ticker.IsRunning());
  EXPECT_FALSE(ticker.Stop());  // Can't stop when it isn't running.
}

TEST(WorkerThreadTickerTest, Callback) {
  base::MessageLoop loop;

  TestCallback callback;
  WorkerThreadTicker ticker(50);
  ASSERT_TRUE(ticker.RegisterTickHandler(&callback));

  ASSERT_TRUE(ticker.Start());
  RunMessageLoopForAWhile();
  EXPECT_TRUE(callback.counter() > 0);

  ASSERT_TRUE(ticker.Stop());
  const int counter_value = callback.counter();
  RunMessageLoopForAWhile();
  EXPECT_EQ(counter_value, callback.counter());
}

TEST(WorkerThreadTickerTest, OutOfScope) {
  base::MessageLoop loop;

  TestCallback callback;
  {
    WorkerThreadTicker ticker(50);
    ASSERT_TRUE(ticker.RegisterTickHandler(&callback));

    ASSERT_TRUE(ticker.Start());
    RunMessageLoopForAWhile();
    EXPECT_TRUE(callback.counter() > 0);
  }
  const int counter_value = callback.counter();
  RunMessageLoopForAWhile();
  EXPECT_EQ(counter_value, callback.counter());
}

TEST(WorkerThreadTickerTest, LongCallback) {
  base::MessageLoop loop;

  LongCallback callback;
  WorkerThreadTicker ticker(50);
  ASSERT_TRUE(ticker.RegisterTickHandler(&callback));

  ASSERT_TRUE(ticker.Start());
  RunMessageLoopForAWhile();
}

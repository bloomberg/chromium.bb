// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"

#include "base/compiler_specific.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(WaitableEventTest, ManualBasics) {
  WaitableEvent event(true, false);

  EXPECT_FALSE(event.IsSignaled());

  event.Signal();
  EXPECT_TRUE(event.IsSignaled());
  EXPECT_TRUE(event.IsSignaled());

  event.Reset();
  EXPECT_FALSE(event.IsSignaled());
  EXPECT_FALSE(event.TimedWait(TimeDelta::FromMilliseconds(10)));

  event.Signal();
  event.Wait();
  EXPECT_TRUE(event.TimedWait(TimeDelta::FromMilliseconds(10)));
}

TEST(WaitableEventTest, AutoBasics) {
  WaitableEvent event(false, false);

  EXPECT_FALSE(event.IsSignaled());

  event.Signal();
  EXPECT_TRUE(event.IsSignaled());
  EXPECT_FALSE(event.IsSignaled());

  event.Reset();
  EXPECT_FALSE(event.IsSignaled());
  EXPECT_FALSE(event.TimedWait(TimeDelta::FromMilliseconds(10)));

  event.Signal();
  event.Wait();
  EXPECT_FALSE(event.TimedWait(TimeDelta::FromMilliseconds(10)));

  event.Signal();
  EXPECT_TRUE(event.TimedWait(TimeDelta::FromMilliseconds(10)));
}

TEST(WaitableEventTest, WaitManyShortcut) {
  WaitableEvent* ev[5];
  for (unsigned i = 0; i < 5; ++i)
    ev[i] = new WaitableEvent(false, false);

  ev[3]->Signal();
  EXPECT_EQ(WaitableEvent::WaitMany(ev, 5), 3u);

  ev[3]->Signal();
  EXPECT_EQ(WaitableEvent::WaitMany(ev, 5), 3u);

  ev[4]->Signal();
  EXPECT_EQ(WaitableEvent::WaitMany(ev, 5), 4u);

  ev[0]->Signal();
  EXPECT_EQ(WaitableEvent::WaitMany(ev, 5), 0u);

  for (unsigned i = 0; i < 5; ++i)
    delete ev[i];
}

class WaitableEventSignaler : public PlatformThread::Delegate {
 public:
  WaitableEventSignaler(double seconds, WaitableEvent* ev)
      : seconds_(seconds),
        ev_(ev) {
  }

  void ThreadMain() override {
    PlatformThread::Sleep(TimeDelta::FromSecondsD(seconds_));
    ev_->Signal();
  }

 private:
  const double seconds_;
  WaitableEvent *const ev_;
};

TEST(WaitableEventTest, WaitAndDelete) {
  // This test tests that if a WaitableEvent can be safely deleted
  // when |Wait| is done without additional synchrnization.
  // If this test crashes, it is a bug.

  WaitableEvent* ev = new WaitableEvent(false, false);

  WaitableEventSignaler signaler(0.01, ev);
  PlatformThreadHandle thread;
  PlatformThread::Create(0, &signaler, &thread);

  ev->Wait();
  delete ev;

  PlatformThread::Join(thread);
}

TEST(WaitableEventTest, WaitMany) {
  // This test tests that if a WaitableEvent can be safely deleted
  // when |WaitMany| is done without additional synchrnization.
  // If this test crashes, it is a bug.

  WaitableEvent* ev[5];
  for (unsigned i = 0; i < 5; ++i)
    ev[i] = new WaitableEvent(false, false);

  WaitableEventSignaler signaler(0.01, ev[2]);
  PlatformThreadHandle thread;
  PlatformThread::Create(0, &signaler, &thread);

  size_t index = WaitableEvent::WaitMany(ev, 5);

  for (unsigned i = 0; i < 5; ++i)
    delete ev[i];

  PlatformThread::Join(thread);
  EXPECT_EQ(2u, index);
}

}  // namespace base

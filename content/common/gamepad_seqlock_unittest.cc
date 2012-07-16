// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gamepad_seqlock.h"

#include <stdlib.h>

#include "base/atomic_ref_count.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Basic test to make sure that basic operation works correctly.

struct TestData {
  unsigned a, b, c;
};

class BasicSeqLockTestThread : public PlatformThread::Delegate {
 public:
  BasicSeqLockTestThread() {}

  void Init(
      content::GamepadSeqLock<TestData>* seqlock,
      base::subtle::Atomic32* ready) {
    seqlock_ = seqlock;
    ready_ = ready;
  }
  virtual void ThreadMain() {
    while (AtomicRefCountIsZero(ready_)) {
      PlatformThread::YieldCurrentThread();
    }

    for (unsigned i = 0; i < 1000; ++i) {
      TestData copy;
      seqlock_->ReadTo(&copy);
      EXPECT_EQ(copy.a + 100, copy.b);
      EXPECT_EQ(copy.c, copy.b + copy.a);
    }

    AtomicRefCountDec(ready_);
  }

 private:
  content::GamepadSeqLock<TestData>* seqlock_;
  base::AtomicRefCount* ready_;

  DISALLOW_COPY_AND_ASSIGN(BasicSeqLockTestThread);
};

TEST(GamepadSeqLockTest, ManyThreads) {
  content::GamepadSeqLock<TestData> seqlock;
  TestData data = { 0, 0, 0 };
  base::AtomicRefCount ready = 0;

  static const unsigned kNumReaderThreads = 10;
  BasicSeqLockTestThread threads[kNumReaderThreads];
  PlatformThreadHandle handles[kNumReaderThreads];

  for (unsigned i = 0; i < kNumReaderThreads; ++i)
    threads[i].Init(&seqlock, &ready);
  for (unsigned i = 0; i < kNumReaderThreads; ++i)
    ASSERT_TRUE(PlatformThread::Create(0, &threads[i], &handles[i]));

  // The main thread is the writer, and the spawned are readers.
  unsigned counter = 0;
  for (;;) {
    data.a = counter++;
    data.b = data.a + 100;
    data.c = data.b + data.a;
    seqlock.Write(data);

    if (counter == 1)
      base::AtomicRefCountIncN(&ready, kNumReaderThreads);

    if (AtomicRefCountIsZero(&ready))
      break;
  }

  for (unsigned i = 0; i < kNumReaderThreads; ++i)
    PlatformThread::Join(handles[i]);
}

}  // namespace base

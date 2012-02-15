// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_net_log.h"

#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kThreads = 10;
const int kEvents = 100;

class ChromeNetLogTestThread : public base::SimpleThread {
 public:
  ChromeNetLogTestThread() : base::SimpleThread("ChromeNetLogTest"),
                             can_start_loop_(false, false),
                             log_(NULL) {
  }

  void Init(ChromeNetLog* log) {
    log_ = log;
  }

  virtual void Run() {
    can_start_loop_.Wait();
    for (int i = 0; i < kEvents; ++i) {
      net::NetLog::Source source(net::NetLog::SOURCE_SOCKET, log_->NextID());
      log_->AddEntry(net::NetLog::TYPE_SOCKET_ALIVE, base::TimeTicks(),
                     source, net::NetLog::PHASE_BEGIN, NULL);
    }
  }

  void ReallyStart() {
    can_start_loop_.Signal();
  }

 private:
  // Only triggered once all threads have been created, to make it much less
  // likely each thread completes before the next one starts.
  base::WaitableEvent can_start_loop_;

  ChromeNetLog* log_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLogTestThread);
};

class CountingObserver : public ChromeNetLog::ThreadSafeObserverImpl {
 public:
  CountingObserver()
      : ChromeNetLog::ThreadSafeObserverImpl(net::NetLog::LOG_ALL),
        count_(0) {}

  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params) OVERRIDE {
    count_++;
  }

  int count() const { return count_; }

 private:
  int count_;
};

}  // namespace

// Makes sure that events are dispatched to all observers, and that this
// operation works correctly when run on multiple threads.
TEST(ChromeNetLogTest, NetLogThreads) {
  ChromeNetLog log;
  ChromeNetLogTestThread threads[kThreads];

  // Attach some observers
  CountingObserver observers[3];
  for (size_t i = 0; i < arraysize(observers); ++i)
    observers[i].AddAsObserver(&log);

  // Run a bunch of threads to completion, each of which will emit events to
  // |log|.
  for (int i = 0; i < kThreads; ++i) {
    threads[i].Init(&log);
    threads[i].Start();
  }

  for (int i = 0; i < kThreads; ++i)
    threads[i].ReallyStart();

  for (int i = 0; i < kThreads; ++i)
    threads[i].Join();

  // Check that each observer saw the emitted events.
  const int kTotalEvents = kThreads * kEvents;
  for (size_t i = 0; i < arraysize(observers); ++i)
    EXPECT_EQ(kTotalEvents, observers[i].count());

  // Detach all the observers
  for (size_t i = 0; i < arraysize(observers); ++i)
    observers[i].RemoveAsObserver();
}

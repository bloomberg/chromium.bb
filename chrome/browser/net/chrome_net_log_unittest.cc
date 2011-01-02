// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
    log_->ClearAllPassivelyCapturedEvents();
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

}  // namespace

// Attempts to check thread safety, exercising checks in ChromeNetLog and
// PassiveLogCollector.
TEST(ChromeNetLogTest, NetLogThreads) {
  ChromeNetLog log;
  ChromeNetLogTestThread threads[kThreads];

  for (int i = 0; i < kThreads; ++i) {
    threads[i].Init(&log);
    threads[i].Start();
  }

  for (int i = 0; i < kThreads; ++i)
    threads[i].ReallyStart();

  for (int i = 0; i < kThreads; ++i)
    threads[i].Join();

  ChromeNetLog::EntryList entries;
  log.GetAllPassivelyCapturedEvents(&entries);
  EXPECT_EQ(0u, entries.size());
}

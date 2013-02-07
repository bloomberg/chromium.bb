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

class CountingObserver : public net::NetLog::ThreadSafeObserver {
 public:
  CountingObserver() : count_(0) {}

  virtual ~CountingObserver() {
    if (net_log())
      net_log()->RemoveThreadSafeObserver(this);
  }

  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE {
    ++count_;
  }

  int count() const { return count_; }

 private:
  int count_;
};

void AddEvent(ChromeNetLog* net_log) {
  net_log->AddGlobalEntry(net::NetLog::TYPE_CANCELLED);
}

// A thread that waits until an event has been signalled before calling
// RunTestThread.
class ChromeNetLogTestThread : public base::SimpleThread {
 public:
  ChromeNetLogTestThread() : base::SimpleThread("ChromeNetLogTest"),
                             net_log_(NULL),
                             start_event_(NULL) {
  }

  // We'll wait for |start_event| to be triggered before calling a subclass's
  // subclass's RunTestThread() function.
  void Init(ChromeNetLog* net_log, base::WaitableEvent* start_event) {
    start_event_ = start_event;
    net_log_ = net_log;
  }

  virtual void Run() OVERRIDE {
    start_event_->Wait();
    RunTestThread();
  }

  // Subclasses must override this with the code they want to run on their
  // thread.
  virtual void RunTestThread() = 0;

 protected:
  ChromeNetLog* net_log_;

 private:
  // Only triggered once all threads have been created, to make it less likely
  // each thread completes before the next one starts.
  base::WaitableEvent* start_event_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLogTestThread);
};

// A thread that adds a bunch of events to the NetLog.
class AddEventsTestThread : public ChromeNetLogTestThread {
 public:
  AddEventsTestThread() {}
  virtual ~AddEventsTestThread() {}

 private:
  virtual void RunTestThread() OVERRIDE {
    for (int i = 0; i < kEvents; ++i)
      AddEvent(net_log_);
  }

  DISALLOW_COPY_AND_ASSIGN(AddEventsTestThread);
};

// A thread that adds and removes an observer from the NetLog repeatedly.
class AddRemoveObserverTestThread : public ChromeNetLogTestThread {
 public:
  AddRemoveObserverTestThread() {}

  virtual ~AddRemoveObserverTestThread() {
    EXPECT_TRUE(!observer_.net_log());
  }

 private:
  virtual void RunTestThread() OVERRIDE {
    for (int i = 0; i < kEvents; ++i) {
      ASSERT_FALSE(observer_.net_log());

      net_log_->AddThreadSafeObserver(&observer_, net::NetLog::LOG_BASIC);
      ASSERT_EQ(net_log_, observer_.net_log());
      ASSERT_EQ(net::NetLog::LOG_BASIC, observer_.log_level());

      net_log_->SetObserverLogLevel(&observer_, net::NetLog::LOG_ALL_BUT_BYTES);
      ASSERT_EQ(net_log_, observer_.net_log());
      ASSERT_EQ(net::NetLog::LOG_ALL_BUT_BYTES, observer_.log_level());
      ASSERT_LE(net_log_->GetLogLevel(), net::NetLog::LOG_ALL_BUT_BYTES);

      net_log_->SetObserverLogLevel(&observer_, net::NetLog::LOG_ALL);
      ASSERT_EQ(net_log_, observer_.net_log());
      ASSERT_EQ(net::NetLog::LOG_ALL, observer_.log_level());
      ASSERT_LE(net_log_->GetLogLevel(), net::NetLog::LOG_ALL);

      net_log_->RemoveThreadSafeObserver(&observer_);
      ASSERT_TRUE(!observer_.net_log());
    }
  }

  CountingObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(AddRemoveObserverTestThread);
};

// Creates |kThreads| threads of type |ThreadType| and then runs them all
// to completion.
template<class ThreadType>
void RunTestThreads(ChromeNetLog* net_log) {
  ThreadType threads[kThreads];
  base::WaitableEvent start_event(true, false);

  for (size_t i = 0; i < arraysize(threads); ++i) {
    threads[i].Init(net_log, &start_event);
    threads[i].Start();
  }

  start_event.Signal();

  for (size_t i = 0; i < arraysize(threads); ++i)
    threads[i].Join();
}

// Makes sure that events on multiple threads are dispatched to all observers.
TEST(ChromeNetLogTest, NetLogEventThreads) {
  ChromeNetLog net_log;

  // Attach some observers.  Since they're created after |net_log|, they'll
  // safely detach themselves on destruction.
  CountingObserver observers[3];
  for (size_t i = 0; i < arraysize(observers); ++i)
    net_log.AddThreadSafeObserver(&observers[i], net::NetLog::LOG_BASIC);

  // Run a bunch of threads to completion, each of which will emit events to
  // |net_log|.
  RunTestThreads<AddEventsTestThread>(&net_log);

  // Check that each observer saw the emitted events.
  const int kTotalEvents = kThreads * kEvents;
  for (size_t i = 0; i < arraysize(observers); ++i)
    EXPECT_EQ(kTotalEvents, observers[i].count());
}

// Test adding and removing a single observer.
TEST(ChromeNetLogTest, NetLogAddRemoveObserver) {
  ChromeNetLog net_log;
  CountingObserver observer;

  AddEvent(&net_log);
  EXPECT_EQ(0, observer.count());
  EXPECT_EQ(NULL, observer.net_log());
  EXPECT_EQ(net::NetLog::LOG_BASIC, net_log.GetLogLevel());

  // Add the observer and add an event.
  net_log.AddThreadSafeObserver(&observer, net::NetLog::LOG_BASIC);
  EXPECT_EQ(&net_log, observer.net_log());
  EXPECT_EQ(net::NetLog::LOG_BASIC, observer.log_level());
  EXPECT_EQ(net::NetLog::LOG_BASIC, net_log.GetLogLevel());

  AddEvent(&net_log);
  EXPECT_EQ(1, observer.count());

  // Change the observer's logging level and add an event.
  net_log.SetObserverLogLevel(&observer, net::NetLog::LOG_ALL);
  EXPECT_EQ(&net_log, observer.net_log());
  EXPECT_EQ(net::NetLog::LOG_ALL, observer.log_level());
  EXPECT_EQ(net::NetLog::LOG_ALL, net_log.GetLogLevel());

  AddEvent(&net_log);
  EXPECT_EQ(2, observer.count());

  // Remove observer and add an event.
  net_log.RemoveThreadSafeObserver(&observer);
  EXPECT_EQ(NULL, observer.net_log());
  EXPECT_EQ(net::NetLog::LOG_BASIC, net_log.GetLogLevel());

  AddEvent(&net_log);
  EXPECT_EQ(2, observer.count());

  // Add the observer a final time, and add an event.
  net_log.AddThreadSafeObserver(&observer, net::NetLog::LOG_ALL);
  EXPECT_EQ(&net_log, observer.net_log());
  EXPECT_EQ(net::NetLog::LOG_ALL, observer.log_level());
  EXPECT_EQ(net::NetLog::LOG_ALL, net_log.GetLogLevel());

  AddEvent(&net_log);
  EXPECT_EQ(3, observer.count());
}

// Test adding and removing two observers.
TEST(ChromeNetLogTest, NetLogTwoObservers) {
  ChromeNetLog net_log;
  CountingObserver observer[2];

  // Add first observer.
  net_log.AddThreadSafeObserver(&observer[0], net::NetLog::LOG_ALL_BUT_BYTES);
  EXPECT_EQ(&net_log, observer[0].net_log());
  EXPECT_EQ(NULL, observer[1].net_log());
  EXPECT_EQ(net::NetLog::LOG_ALL_BUT_BYTES, observer[0].log_level());
  EXPECT_EQ(net::NetLog::LOG_ALL_BUT_BYTES, net_log.GetLogLevel());

  // Add second observer observer.
  net_log.AddThreadSafeObserver(&observer[1], net::NetLog::LOG_ALL);
  EXPECT_EQ(&net_log, observer[0].net_log());
  EXPECT_EQ(&net_log, observer[1].net_log());
  EXPECT_EQ(net::NetLog::LOG_ALL_BUT_BYTES, observer[0].log_level());
  EXPECT_EQ(net::NetLog::LOG_ALL, observer[1].log_level());
  EXPECT_EQ(net::NetLog::LOG_ALL, net_log.GetLogLevel());

  // Add event and make sure both observers receive it.
  AddEvent(&net_log);
  EXPECT_EQ(1, observer[0].count());
  EXPECT_EQ(1, observer[1].count());

  // Remove second observer.
  net_log.RemoveThreadSafeObserver(&observer[1]);
  EXPECT_EQ(&net_log, observer[0].net_log());
  EXPECT_EQ(NULL, observer[1].net_log());
  EXPECT_EQ(net::NetLog::LOG_ALL_BUT_BYTES, observer[0].log_level());
  EXPECT_EQ(net::NetLog::LOG_ALL_BUT_BYTES, net_log.GetLogLevel());

  // Add event and make sure only second observer gets it.
  AddEvent(&net_log);
  EXPECT_EQ(2, observer[0].count());
  EXPECT_EQ(1, observer[1].count());

  // Remove first observer.
  net_log.RemoveThreadSafeObserver(&observer[0]);
  EXPECT_EQ(NULL, observer[0].net_log());
  EXPECT_EQ(NULL, observer[1].net_log());
  EXPECT_EQ(net::NetLog::LOG_BASIC, net_log.GetLogLevel());

  // Add event and make sure neither observer gets it.
  AddEvent(&net_log);
  EXPECT_EQ(2, observer[0].count());
  EXPECT_EQ(1, observer[1].count());
}

// Makes sure that adding and removing observers simultaneously on different
// threads works.
TEST(ChromeNetLogTest, NetLogAddRemoveObserverThreads) {
  ChromeNetLog net_log;

  // Run a bunch of threads to completion, each of which will repeatedly add
  // and remove an observer, and set its logging level.
  RunTestThreads<AddRemoveObserverTestThread>(&net_log);
}

}  // namespace

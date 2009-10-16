// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iosfwd>
#include <sstream>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/port.h"
#include "build/build_config.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::endl;
using std::ostream;
using std::string;
using std::stringstream;
using std::vector;

namespace {

class Pair;

struct TestEvent {
  Pair* source;
  enum {
    A_CHANGED, B_CHANGED, PAIR_BEING_DELETED,
  } what_happened;
  int old_value;
};

struct TestEventTraits {
  typedef TestEvent EventType;
  static bool IsChannelShutdownEvent(const TestEvent& event) {
    return TestEvent::PAIR_BEING_DELETED == event.what_happened;
  }
};

class Pair {
 public:
  typedef EventChannel<TestEventTraits> Channel;
  explicit Pair(const string& name) : name_(name), a_(0), b_(0) {
    TestEvent shutdown = { this, TestEvent::PAIR_BEING_DELETED, 0 };
    event_channel_ = new Channel(shutdown);
  }
  ~Pair() {
    delete event_channel_;
  }
  void set_a(int n) {
    TestEvent event = { this, TestEvent::A_CHANGED, a_ };
    a_ = n;
    event_channel_->NotifyListeners(event);
  }
  void set_b(int n) {
    TestEvent event = { this, TestEvent::B_CHANGED, b_ };
    b_ = n;
    event_channel_->NotifyListeners(event);
  }
  int a() const { return a_; }
  int b() const { return b_; }
  const string& name() { return name_; }
  Channel* event_channel() const { return event_channel_; }

 protected:
  const string name_;
  int a_;
  int b_;
  Channel* event_channel_;
};

class EventLogger {
 public:
  explicit EventLogger(ostream& out) : out_(out) { }
  ~EventLogger() {
    for (Hookups::iterator i = hookups_.begin(); i != hookups_.end(); ++i)
      delete *i;
  }

  void Hookup(const string name, Pair::Channel* channel) {
    hookups_.push_back(NewEventListenerHookup(channel, this,
                                              &EventLogger::HandlePairEvent,
                                              name));
  }

  void HandlePairEvent(const string& name, const TestEvent& event) {
    const char* what_changed = NULL;
    int new_value = 0;
    Hookups::iterator dead;
    switch (event.what_happened) {
    case TestEvent::A_CHANGED:
      what_changed = "A";
      new_value = event.source->a();
      break;
    case TestEvent::B_CHANGED:
      what_changed = "B";
      new_value = event.source->b();
      break;
    case TestEvent::PAIR_BEING_DELETED:
      out_ << name << " heard " << event.source->name() << " being deleted."
           << endl;
      return;
    default:
      LOG(FATAL) << "Bad event.what_happened: " << event.what_happened;
      break;
    }
    out_ << name << " heard " << event.source->name() << "'s " << what_changed
         << " change from "
         << event.old_value << " to " << new_value << endl;
  }

  typedef vector<EventListenerHookup*> Hookups;
  Hookups hookups_;
  ostream& out_;
};

const char golden_result[] = "Larry heard Sally's B change from 0 to 2\n"
"Larry heard Sally's A change from 1 to 3\n"
"Lewis heard Sam's B change from 0 to 5\n"
"Larry heard Sally's A change from 3 to 6\n"
"Larry heard Sally being deleted.\n";

TEST(EventSys, Basic) {
  Pair sally("Sally"), sam("Sam");
  sally.set_a(1);
  stringstream log;
  EventLogger logger(log);
  logger.Hookup("Larry", sally.event_channel());
  sally.set_b(2);
  sally.set_a(3);
  sam.set_a(4);
  logger.Hookup("Lewis", sam.event_channel());
  sam.set_b(5);
  sally.set_a(6);
  // Test that disconnect within callback doesn't deadlock.
  TestEvent event = {&sally, TestEvent::PAIR_BEING_DELETED, 0 };
  sally.event_channel()->NotifyListeners(event);
  sally.set_a(7);
  ASSERT_EQ(log.str(), golden_result);
}


// This goes pretty far beyond the normal use pattern, so don't use
// ThreadTester as an example of what to do.
class ThreadTester : public EventListener<TestEvent>,
                     public PlatformThread::Delegate {
 public:
  explicit ThreadTester(Pair* pair)
    : pair_(pair), remove_event_(&remove_event_mutex_),
      remove_event_bool_(false)  {
    pair_->event_channel()->AddListener(this);
  }
  ~ThreadTester() {
    pair_->event_channel()->RemoveListener(this);
    for (size_t i = 0; i < threads_.size(); i++) {
      PlatformThread::Join(threads_[i].thread);
    }
  }

  struct ThreadInfo {
    PlatformThreadHandle thread;
    bool* completed;
  };

  struct ThreadArgs {
    ConditionVariable* thread_running_cond;
    Lock* thread_running_mutex;
    bool thread_running;
    bool completed;
  };

  void Go() {
    Lock thread_running_mutex;
    ConditionVariable thread_running_cond(&thread_running_mutex);
    ThreadArgs args;
    ThreadInfo info;
    info.completed = false;
    args.completed = info.completed;
    args.thread_running_cond = &(thread_running_cond);
    args.thread_running_mutex = &(thread_running_mutex);
    args.thread_running = false;
    args_ = args;
    ASSERT_TRUE(PlatformThread::Create(0, this, &info.thread));
    thread_running_mutex.Acquire();
    while ((args_.thread_running) == false) {
      thread_running_cond.Wait();
    }
    thread_running_mutex.Release();
    threads_.push_back(info);
  }

  // PlatformThread::Delegate methods.
  virtual void ThreadMain() {
    // Make sure each thread gets a current MessageLoop in TLS.
    // This test should use chrome threads for testing, but I'll leave it like
    // this for the moment since it requires a big chunk of rewriting and I
    // want the test passing while I checkpoint my CL.  Technically speaking,
    // there should be no functional difference.
    MessageLoop message_loop;
    args_.thread_running_mutex->Acquire();
    args_.thread_running = true;
    args_.thread_running_mutex->Release();
    args_.thread_running_cond->Signal();

    remove_event_mutex_.Acquire();
    while (remove_event_bool_ == false) {
      remove_event_.Wait();
    }
    remove_event_mutex_.Release();

    // Normally, you'd just delete the hookup. This is very bad style, but
    // necessary for the test.
    pair_->event_channel()->RemoveListener(this);
    args_.completed = true;
  }

  void HandleEvent(const TestEvent& event) {
    remove_event_mutex_.Acquire();
    remove_event_bool_ = true;
    remove_event_mutex_.Release();
    remove_event_.Broadcast();

    // Windows and posix use different functions to sleep.
#ifdef OS_WIN
    Sleep(1);
#else
    sleep(1);
#endif

    for (size_t i = 0; i < threads_.size(); i++) {
      if (threads_[i].completed)
        LOG(FATAL) << "A test thread exited too early.";
    }
  }

  Pair* pair_;
  ConditionVariable remove_event_;
  Lock remove_event_mutex_;
  bool remove_event_bool_;
  vector<ThreadInfo> threads_;
  ThreadArgs args_;
};

TEST(EventSys, Multithreaded) {
  Pair sally("Sally");
  ThreadTester a(&sally);
  for (int i = 0; i < 3; ++i)
    a.Go();
  sally.set_b(99);
}

class HookupDeleter {
 public:
  void HandleEvent(const TestEvent& event) {
    delete hookup_;
    hookup_ = NULL;
  }
  EventListenerHookup* hookup_;
};

TEST(EventSys, InHandlerDeletion) {
  Pair sally("Sally");
  HookupDeleter deleter;
  deleter.hookup_ = NewEventListenerHookup(sally.event_channel(),
                                           &deleter,
                                           &HookupDeleter::HandleEvent);
  sally.set_a(1);
  ASSERT_TRUE(NULL == deleter.hookup_);
}

}  // namespace

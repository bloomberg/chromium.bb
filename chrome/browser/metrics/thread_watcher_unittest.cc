// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::TimeDelta;
using base::TimeTicks;

enum State {
  INITIALIZED,        // Created ThreadWatch object.
  ACTIVATED,          // Thread watching activated.
  SENT_PING,          // Sent ping message to watched thread.
  RECEIVED_PONG,      // Received Pong message.
  DEACTIVATED,        // Thread watching de-activated.
};

enum WaitState {
  UNINITIALIZED,
  STARTED_WAITING,    // Start waiting for state_ to change to expected_state.
  STOPPED_WAITING,    // Done with the waiting.
  ALL_DONE,           // Done with waiting for STOPPED_WAITING.
};

enum CheckResponseState {
  UNKNOWN,
  SUCCESSFUL,         // CheckResponse was successful.
  FAILED,             // CheckResponse has failed.
};

// This class helps to track and manipulate thread state during tests. This
// class also has utility method to simulate hanging of watched thread by making
// the watched thread wait for a very long time by posting a task on watched
// thread that keeps it busy. It also has an utility method to block running of
// tests until ThreadWatcher object's post-condition state changes to an
// expected state.
class CustomThreadWatcher : public ThreadWatcher {
 public:
  base::Lock custom_lock_;
  base::ConditionVariable state_changed_;
  State thread_watcher_state_;
  WaitState wait_state_;
  CheckResponseState check_response_state_;
  uint64 ping_sent_;
  uint64 pong_received_;
  uint64 success_response_;
  uint64 failed_response_;
  base::TimeTicks saved_ping_time_;
  uint64 saved_ping_sequence_number_;

  CustomThreadWatcher(const BrowserThread::ID thread_id,
                      const std::string thread_name,
                      const TimeDelta& sleep_time,
                      const TimeDelta& unresponsive_time)
      : ThreadWatcher(thread_id, thread_name, sleep_time, unresponsive_time),
        state_changed_(&custom_lock_),
        thread_watcher_state_(INITIALIZED),
        wait_state_(UNINITIALIZED),
        check_response_state_(UNKNOWN),
        ping_sent_(0),
        pong_received_(0),
        success_response_(0),
        failed_response_(0),
        saved_ping_time_(base::TimeTicks::Now()),
        saved_ping_sequence_number_(0) {
  }

  State UpdateState(State new_state) {
    State old_state;
    {
      base::AutoLock auto_lock(custom_lock_);
      old_state = thread_watcher_state_;
      if (old_state != DEACTIVATED)
        thread_watcher_state_ = new_state;
      if (new_state == SENT_PING)
        ++ping_sent_;
      if (new_state == RECEIVED_PONG)
        ++pong_received_;
      saved_ping_time_ = ping_time();
      saved_ping_sequence_number_ = ping_sequence_number();
    }
    state_changed_.Broadcast();
    return old_state;
  }

  WaitState UpdateWaitState(WaitState new_state) {
    WaitState old_state;
    {
      base::AutoLock auto_lock(custom_lock_);
      old_state = wait_state_;
      wait_state_ = new_state;
    }
    state_changed_.Broadcast();
    return old_state;
  }

  void ActivateThreadWatching() {
    State old_state = UpdateState(ACTIVATED);
    EXPECT_EQ(old_state, INITIALIZED);
    ThreadWatcher::ActivateThreadWatching();
  }

  void DeActivateThreadWatching() {
    State old_state = UpdateState(DEACTIVATED);
    EXPECT_TRUE(old_state == ACTIVATED || old_state == SENT_PING ||
                old_state == RECEIVED_PONG);
    ThreadWatcher::DeActivateThreadWatching();
  }

  void PostPingMessage() {
    State old_state = UpdateState(SENT_PING);
    EXPECT_TRUE(old_state == ACTIVATED || old_state == RECEIVED_PONG);
    ThreadWatcher::PostPingMessage();
  }

  void OnPongMessage(uint64 ping_sequence_number) {
    State old_state = UpdateState(RECEIVED_PONG);
    EXPECT_TRUE(old_state == SENT_PING || old_state == DEACTIVATED);
    ThreadWatcher::OnPongMessage(ping_sequence_number);
  }

  bool OnCheckResponsiveness(uint64 ping_sequence_number) {
    bool responsive =
        ThreadWatcher::OnCheckResponsiveness(ping_sequence_number);
    {
      base::AutoLock auto_lock(custom_lock_);
      if (responsive) {
        ++success_response_;
        check_response_state_ = SUCCESSFUL;
      } else {
        ++failed_response_;
        check_response_state_ = FAILED;
      }
    }
    // Broadcast to indicate we have checked responsiveness of the thread that
    // is watched.
    state_changed_.Broadcast();
    return responsive;
  }

  void WaitForWaitStateChange(TimeDelta wait_time, WaitState expected_state) {
    DCHECK(!WatchDogThread::CurrentlyOnWatchDogThread());
    TimeTicks end_time = TimeTicks::Now() + wait_time;
    {
      base::AutoLock auto_lock(custom_lock_);
      while (wait_state_ != expected_state && TimeTicks::Now() < end_time)
        state_changed_.TimedWait(end_time - TimeTicks::Now());
    }
  }

  void VeryLongMethod(TimeDelta wait_time) {
    DCHECK(!WatchDogThread::CurrentlyOnWatchDogThread());
    WaitForWaitStateChange(wait_time, STOPPED_WAITING);
    UpdateWaitState(ALL_DONE);
  }

  State WaitForStateChange(const TimeDelta& wait_time, State expected_state) {
    DCHECK(!WatchDogThread::CurrentlyOnWatchDogThread());
    UpdateWaitState(STARTED_WAITING);

    State exit_state;
    // Keep the thread that is running the tests waiting until ThreadWatcher
    // object's state changes to the expected_state or until wait_time elapses.
    for (int i = 0; i < 3; ++i) {
        TimeTicks end_time = TimeTicks::Now() + wait_time;
        {
          base::AutoLock auto_lock(custom_lock_);
          while (thread_watcher_state_ != expected_state &&
                 TimeTicks::Now() < end_time) {
            TimeDelta state_change_wait_time = end_time - TimeTicks::Now();
            state_changed_.TimedWait(state_change_wait_time);
          }
          // Capture the thread_watcher_state_ before it changes and return it
          // to the caller.
          exit_state = thread_watcher_state_;
          if (exit_state == expected_state)
            break;
        }
    }
    UpdateWaitState(STOPPED_WAITING);
    return exit_state;
  }

  CheckResponseState WaitForCheckResponse(const TimeDelta& wait_time,
                                          CheckResponseState expected_state) {
    DCHECK(!WatchDogThread::CurrentlyOnWatchDogThread());
    UpdateWaitState(STARTED_WAITING);

    CheckResponseState exit_state;
    // Keep the thread that is running the tests waiting until ThreadWatcher
    // object's check_response_state_ changes to the expected_state or until
    // wait_time elapses.
    for (int i = 0; i < 3; ++i) {
        TimeTicks end_time = TimeTicks::Now() + wait_time;
        {
          base::AutoLock auto_lock(custom_lock_);
          while (check_response_state_ != expected_state &&
                 TimeTicks::Now() < end_time) {
            TimeDelta state_change_wait_time = end_time - TimeTicks::Now();
            state_changed_.TimedWait(state_change_wait_time);
          }
          // Capture the check_response_state_ before it changes and return it
          // to the caller.
          exit_state = check_response_state_;
          if (exit_state == expected_state)
            break;
        }
    }
    UpdateWaitState(STOPPED_WAITING);
    return exit_state;
  }
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(CustomThreadWatcher);

class ThreadWatcherTest : public ::testing::Test {
 public:
  static const TimeDelta kSleepTime;
  static const TimeDelta kUnresponsiveTime;
  static const BrowserThread::ID io_thread_id;
  static const std::string io_thread_name;
  static const BrowserThread::ID webkit_thread_id;
  static const std::string webkit_thread_name;
  CustomThreadWatcher* io_watcher_;
  CustomThreadWatcher* webkit_watcher_;

  ThreadWatcherTest() {
    webkit_thread_.reset(new BrowserThread(BrowserThread::WEBKIT));
    io_thread_.reset(new BrowserThread(BrowserThread::IO));
    watchdog_thread_.reset(new WatchDogThread());
    webkit_thread_->Start();
    io_thread_->Start();
    watchdog_thread_->Start();

    // Setup the registry for thread watchers.
    thread_watcher_list_ = new ThreadWatcherList();

    // Create thread watcher object for the IO thread.
    io_watcher_ = new CustomThreadWatcher(io_thread_id, io_thread_name,
                                          kSleepTime, kUnresponsiveTime);

    // Create thread watcher object for the WEBKIT thread.
    webkit_watcher_ = new CustomThreadWatcher(
        webkit_thread_id, webkit_thread_name, kSleepTime, kUnresponsiveTime);
  }

  ~ThreadWatcherTest() {
    ThreadWatcherList::StopWatchingAll();
    io_watcher_ = NULL;
    webkit_watcher_ = NULL;
    io_thread_.reset();
    webkit_thread_.reset();
    watchdog_thread_.reset();
    delete thread_watcher_list_;
  }

 private:
  scoped_ptr<BrowserThread> webkit_thread_;
  scoped_ptr<BrowserThread> io_thread_;
  scoped_ptr<WatchDogThread> watchdog_thread_;
  ThreadWatcherList* thread_watcher_list_;
};

// Define static constants.
const TimeDelta ThreadWatcherTest::kSleepTime =
    TimeDelta::FromMilliseconds(50);
const TimeDelta ThreadWatcherTest::kUnresponsiveTime =
    TimeDelta::FromMilliseconds(500);
const BrowserThread::ID ThreadWatcherTest::io_thread_id = BrowserThread::IO;
const std::string ThreadWatcherTest::io_thread_name = "IO";
const BrowserThread::ID ThreadWatcherTest::webkit_thread_id =
    BrowserThread::WEBKIT;
const std::string ThreadWatcherTest::webkit_thread_name = "WEBKIT";

// Test registration. When thread_watcher_list_ goes out of scope after
// TearDown, all thread watcher objects will be deleted.
TEST_F(ThreadWatcherTest, Registration) {
  EXPECT_EQ(io_watcher_, ThreadWatcherList::Find(io_thread_id));
  EXPECT_EQ(webkit_watcher_, ThreadWatcherList::Find(webkit_thread_id));

  // Check ThreadWatcher object has all correct parameters.
  EXPECT_EQ(io_thread_id, io_watcher_->thread_id());
  EXPECT_EQ(io_thread_name, io_watcher_->thread_name());
  EXPECT_EQ(kSleepTime, io_watcher_->sleep_time());
  EXPECT_EQ(kUnresponsiveTime, io_watcher_->unresponsive_time());
  EXPECT_FALSE(io_watcher_->active());

  // Check ThreadWatcher object of watched WEBKIT thread has correct data.
  EXPECT_EQ(webkit_thread_id, webkit_watcher_->thread_id());
  EXPECT_EQ(webkit_thread_name, webkit_watcher_->thread_name());
  EXPECT_EQ(kSleepTime, webkit_watcher_->sleep_time());
  EXPECT_EQ(kUnresponsiveTime, webkit_watcher_->unresponsive_time());
  EXPECT_FALSE(webkit_watcher_->active());
}

// Test ActivateThreadWatching and DeActivateThreadWatching of IO thread. This
// method also checks that pong message was sent by the watched thread and pong
// message was received by the WatchDogThread. It also checks that
// OnCheckResponsiveness has verified the ping-pong mechanism and the watched
// thread is not hung.
TEST_F(ThreadWatcherTest, ThreadResponding) {
  TimeTicks time_before_ping = TimeTicks::Now();
  // Activate watching IO thread.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(io_watcher_, &ThreadWatcher::ActivateThreadWatching));

  // Activate would have started ping/pong messaging. Expect atleast one
  // ping/pong messaging sequence to happen.
  io_watcher_->WaitForStateChange(kSleepTime + TimeDelta::FromMinutes(1),
                                  RECEIVED_PONG);
  EXPECT_GT(io_watcher_->ping_sent_, static_cast<uint64>(0));
  EXPECT_GT(io_watcher_->pong_received_, static_cast<uint64>(0));
  EXPECT_TRUE(io_watcher_->active());
  EXPECT_GE(io_watcher_->saved_ping_time_, time_before_ping);
  EXPECT_GE(io_watcher_->saved_ping_sequence_number_, static_cast<uint64>(0));

  // Verify watched thread is responding with ping/pong messaging.
  io_watcher_->WaitForCheckResponse(
      kUnresponsiveTime + TimeDelta::FromMinutes(1), SUCCESSFUL);
  EXPECT_GT(io_watcher_->success_response_, static_cast<uint64>(0));
  EXPECT_EQ(io_watcher_->failed_response_, static_cast<uint64>(0));

  // DeActivate thread watching for shutdown.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(io_watcher_, &ThreadWatcher::DeActivateThreadWatching));
}

// This test posts a task on watched thread that takes very long time (this is
// to simulate hanging of watched thread). It then checks for
// OnCheckResponsiveness raising an alert (OnCheckResponsiveness returns false
// if the watched thread is not responding).
TEST_F(ThreadWatcherTest, ThreadNotResponding) {
  // Simulate hanging of watched thread by making the watched thread wait for a
  // very long time by posting a task on watched thread that keeps it busy.
  BrowserThread::PostTask(
      io_thread_id,
      FROM_HERE,
      NewRunnableMethod(
          io_watcher_,
          &CustomThreadWatcher::VeryLongMethod,
          kUnresponsiveTime * 10));

  // Activate thread watching.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(io_watcher_, &ThreadWatcher::ActivateThreadWatching));

  // Verify watched thread is not responding for ping messages.
  io_watcher_->WaitForCheckResponse(
      kUnresponsiveTime + TimeDelta::FromMinutes(1), FAILED);
  EXPECT_EQ(io_watcher_->success_response_, static_cast<uint64>(0));
  EXPECT_GT(io_watcher_->failed_response_, static_cast<uint64>(0));

  // DeActivate thread watching for shutdown.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(io_watcher_, &ThreadWatcher::DeActivateThreadWatching));

  // Wait for the io_watcher_'s VeryLongMethod to finish.
  io_watcher_->WaitForWaitStateChange(kUnresponsiveTime * 10, ALL_DONE);
}

// Test watching of multiple threads with all threads not responding.
TEST_F(ThreadWatcherTest, MultipleThreadsResponding) {
  // Check for WEBKIT thread to perform ping/pong messaging.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(
          webkit_watcher_, &ThreadWatcher::ActivateThreadWatching));

  // Check for IO thread to perform ping/pong messaging.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(io_watcher_, &ThreadWatcher::ActivateThreadWatching));

  // Verify WEBKIT thread is responding with ping/pong messaging.
  webkit_watcher_->WaitForCheckResponse(
      kUnresponsiveTime + TimeDelta::FromMinutes(1), SUCCESSFUL);
  EXPECT_GT(webkit_watcher_->ping_sent_, static_cast<uint64>(0));
  EXPECT_GT(webkit_watcher_->pong_received_, static_cast<uint64>(0));
  EXPECT_GE(webkit_watcher_->ping_sequence_number_, static_cast<uint64>(0));
  EXPECT_GT(webkit_watcher_->success_response_, static_cast<uint64>(0));
  EXPECT_EQ(webkit_watcher_->failed_response_, static_cast<uint64>(0));

  // Verify IO thread is responding with ping/pong messaging.
  io_watcher_->WaitForCheckResponse(
      kUnresponsiveTime + TimeDelta::FromMinutes(1), SUCCESSFUL);
  EXPECT_GT(io_watcher_->ping_sent_, static_cast<uint64>(0));
  EXPECT_GT(io_watcher_->pong_received_, static_cast<uint64>(0));
  EXPECT_GE(io_watcher_->ping_sequence_number_, static_cast<uint64>(0));
  EXPECT_GT(io_watcher_->success_response_, static_cast<uint64>(0));
  EXPECT_EQ(io_watcher_->failed_response_, static_cast<uint64>(0));

  // DeActivate thread watching for shutdown.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(io_watcher_, &ThreadWatcher::DeActivateThreadWatching));

  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(
          webkit_watcher_, &ThreadWatcher::DeActivateThreadWatching));
}

// Test watching of multiple threads with one of the threads not responding.
TEST_F(ThreadWatcherTest, MultipleThreadsNotResponding) {
  // Simulate hanging of watched thread by making the watched thread wait for a
  // very long time by posting a task on watched thread that keeps it busy.
  BrowserThread::PostTask(
      io_thread_id,
      FROM_HERE,
      NewRunnableMethod(
          io_watcher_,
          &CustomThreadWatcher::VeryLongMethod,
          kUnresponsiveTime * 10));

  // Activate watching of WEBKIT thread.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(
          webkit_watcher_, &ThreadWatcher::ActivateThreadWatching));

  // Activate watching of IO thread.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(io_watcher_, &ThreadWatcher::ActivateThreadWatching));

  // Verify WEBKIT thread is responding with ping/pong messaging.
  webkit_watcher_->WaitForCheckResponse(
      kUnresponsiveTime + TimeDelta::FromMinutes(1), SUCCESSFUL);
  EXPECT_GT(webkit_watcher_->success_response_, static_cast<uint64>(0));
  EXPECT_EQ(webkit_watcher_->failed_response_, static_cast<uint64>(0));

  // Verify IO thread is not responding for ping messages.
  io_watcher_->WaitForCheckResponse(
      kUnresponsiveTime + TimeDelta::FromMinutes(1), FAILED);
  EXPECT_EQ(io_watcher_->success_response_, static_cast<uint64>(0));
  EXPECT_GT(io_watcher_->failed_response_, static_cast<uint64>(0));

  // DeActivate thread watching for shutdown.
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(io_watcher_, &ThreadWatcher::DeActivateThreadWatching));
  WatchDogThread::PostTask(
      FROM_HERE,
      NewRunnableMethod(
          webkit_watcher_, &ThreadWatcher::DeActivateThreadWatching));

  // Wait for the io_watcher_'s VeryLongMethod to finish.
  io_watcher_->WaitForWaitStateChange(kUnresponsiveTime * 10, ALL_DONE);
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/timers/alarm_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

// Most of these tests have been lifted right out of timer_unittest.cc with only
// cosmetic changes (like replacing calls to MessageLoop::current()->Run() with
// a RunLoop).  We want the AlarmTimer to be a drop-in replacement for the
// regular Timer so it should pass the same tests as the Timer class.
//
// The only new tests are the .*ConcurrentResetAndTimerFired tests, which test
// that race conditions that can come up in the AlarmTimer::Delegate are
// properly handled.
namespace timers {
namespace {
// The message loops on which each timer should be tested.
const base::MessageLoop::Type testing_message_loops[] = {
  base::MessageLoop::TYPE_DEFAULT,
  base::MessageLoop::TYPE_IO,
#if !defined(OS_IOS)  // iOS does not allow direct running of the UI loop.
  base::MessageLoop::TYPE_UI,
#endif
};

const int kNumTestingMessageLoops = arraysize(testing_message_loops);
const base::TimeDelta kTenMilliseconds = base::TimeDelta::FromMilliseconds(10);

class OneShotAlarmTimerTester {
 public:
  OneShotAlarmTimerTester(bool* did_run, base::TimeDelta delay)
      : did_run_(did_run),
        delay_(delay),
        timer_(new timers::AlarmTimer(false, false)) {
  }
  void Start() {
    timer_->Start(FROM_HERE,
                  delay_,
                  base::Bind(&OneShotAlarmTimerTester::Run,
                             base::Unretained(this)));
  }

 private:
  void Run() {
    *did_run_ = true;

    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

  bool* did_run_;
  const base::TimeDelta delay_;
  scoped_ptr<timers::AlarmTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(OneShotAlarmTimerTester);
};

class OneShotSelfDeletingAlarmTimerTester {
 public:
  OneShotSelfDeletingAlarmTimerTester(bool* did_run, base::TimeDelta delay)
      : did_run_(did_run),
        delay_(delay),
        timer_(new timers::AlarmTimer(false, false)) {
  }
  void Start() {
    timer_->Start(FROM_HERE,
                  delay_,
                  base::Bind(&OneShotSelfDeletingAlarmTimerTester::Run,
                             base::Unretained(this)));
  }

 private:
  void Run() {
    *did_run_ = true;
    timer_.reset();

    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

  bool* did_run_;
  const base::TimeDelta delay_;
  scoped_ptr<timers::AlarmTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(OneShotSelfDeletingAlarmTimerTester);
};

class RepeatingAlarmTimerTester {
 public:
  RepeatingAlarmTimerTester(bool* did_run, base::TimeDelta delay)
      : did_run_(did_run),
        delay_(delay),
        counter_(10),
        timer_(new timers::AlarmTimer(true, true)) {
  }
  void Start() {
    timer_->Start(FROM_HERE,
                  delay_,
                  base::Bind(&RepeatingAlarmTimerTester::Run,
                             base::Unretained(this)));
  }

 private:
  void Run() {
    if (--counter_ == 0) {
      *did_run_ = true;
      timer_->Stop();

      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    }
  }

  bool* did_run_;
  const base::TimeDelta delay_;
  int counter_;
  scoped_ptr<timers::AlarmTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(RepeatingAlarmTimerTester);
};

void RunTest_OneShotAlarmTimer(base::MessageLoop::Type message_loop_type) {
  base::MessageLoop loop(message_loop_type);

  bool did_run = false;
  OneShotAlarmTimerTester f(&did_run, kTenMilliseconds);
  f.Start();

  base::RunLoop().Run();

  EXPECT_TRUE(did_run);
}

void RunTest_OneShotAlarmTimer_Cancel(
    base::MessageLoop::Type message_loop_type) {
  base::MessageLoop loop(message_loop_type);

  bool did_run_a = false;
  OneShotAlarmTimerTester* a = new OneShotAlarmTimerTester(&did_run_a,
                                                           kTenMilliseconds);

  // This should run before the timer expires.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();

  bool did_run_b = false;
  OneShotAlarmTimerTester b(&did_run_b, kTenMilliseconds);
  b.Start();

  base::RunLoop().Run();

  EXPECT_FALSE(did_run_a);
  EXPECT_TRUE(did_run_b);
}

void RunTest_OneShotSelfDeletingAlarmTimer(
    base::MessageLoop::Type message_loop_type) {
  base::MessageLoop loop(message_loop_type);

  bool did_run = false;
  OneShotSelfDeletingAlarmTimerTester f(&did_run, kTenMilliseconds);
  f.Start();

  base::RunLoop().Run();

  EXPECT_TRUE(did_run);
}

void RunTest_RepeatingAlarmTimer(base::MessageLoop::Type message_loop_type,
                                 const base::TimeDelta& delay) {
  base::MessageLoop loop(message_loop_type);

  bool did_run = false;
  RepeatingAlarmTimerTester f(&did_run, delay);
  f.Start();

  base::RunLoop().Run();

  EXPECT_TRUE(did_run);
}

void RunTest_RepeatingAlarmTimer_Cancel(
    base::MessageLoop::Type message_loop_type, const base::TimeDelta& delay) {
  base::MessageLoop loop(message_loop_type);

  bool did_run_a = false;
  RepeatingAlarmTimerTester* a = new RepeatingAlarmTimerTester(&did_run_a,
                                                               delay);

  // This should run before the timer expires.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();

  bool did_run_b = false;
  RepeatingAlarmTimerTester b(&did_run_b, delay);
  b.Start();

  base::RunLoop().Run();

  EXPECT_FALSE(did_run_a);
  EXPECT_TRUE(did_run_b);
}

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of MessageLoop.  That way we are sure
// that timers work properly in all configurations.

TEST(AlarmTimerTest, OneShotAlarmTimer) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_OneShotAlarmTimer(testing_message_loops[i]);
  }
}

TEST(AlarmTimerTest, OneShotAlarmTimer_Cancel) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_OneShotAlarmTimer_Cancel(testing_message_loops[i]);
  }
}

// If underlying timer does not handle this properly, we will crash or fail
// in full page heap environment.
TEST(AlarmTimerTest, OneShotSelfDeletingAlarmTimer) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_OneShotSelfDeletingAlarmTimer(testing_message_loops[i]);
  }
}

TEST(AlarmTimerTest, RepeatingAlarmTimer) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_RepeatingAlarmTimer(testing_message_loops[i],
                                kTenMilliseconds);
  }
}

TEST(AlarmTimerTest, RepeatingAlarmTimer_Cancel) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_RepeatingAlarmTimer_Cancel(testing_message_loops[i],
                                       kTenMilliseconds);
  }
}

TEST(AlarmTimerTest, RepeatingAlarmTimerZeroDelay) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_RepeatingAlarmTimer(testing_message_loops[i],
                                base::TimeDelta::FromMilliseconds(0));
  }
}

TEST(AlarmTimerTest, RepeatingAlarmTimerZeroDelay_Cancel) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_RepeatingAlarmTimer_Cancel(testing_message_loops[i],
                                       base::TimeDelta::FromMilliseconds(0));
  }
}

TEST(AlarmTimerTest, MessageLoopShutdown) {
  // This test is designed to verify that shutdown of the
  // message loop does not cause crashes if there were pending
  // timers not yet fired.  It may only trigger exceptions
  // if debug heap checking is enabled.
  bool did_run = false;
  {
    OneShotAlarmTimerTester a(&did_run, kTenMilliseconds);
    OneShotAlarmTimerTester b(&did_run, kTenMilliseconds);
    OneShotAlarmTimerTester c(&did_run, kTenMilliseconds);
    OneShotAlarmTimerTester d(&did_run, kTenMilliseconds);
    {
      base::MessageLoop loop;
      a.Start();
      b.Start();
    }  // MessageLoop destructs by falling out of scope.
  }  // OneShotTimers destruct.  SHOULD NOT CRASH, of course.

  EXPECT_FALSE(did_run);
}

TEST(AlarmTimerTest, NonRepeatIsRunning) {
  {
    base::MessageLoop loop;
    timers::AlarmTimer timer(false, false);
    EXPECT_FALSE(timer.IsRunning());
    timer.Start(FROM_HERE, base::TimeDelta::FromDays(1),
                base::Bind(&base::DoNothing));
    EXPECT_TRUE(timer.IsRunning());
    timer.Stop();
    EXPECT_FALSE(timer.IsRunning());
    EXPECT_TRUE(timer.user_task().is_null());
  }

  {
    timers::AlarmTimer timer(true, false);
    base::MessageLoop loop;
    EXPECT_FALSE(timer.IsRunning());
    timer.Start(FROM_HERE, base::TimeDelta::FromDays(1),
                base::Bind(&base::DoNothing));
    EXPECT_TRUE(timer.IsRunning());
    timer.Stop();
    EXPECT_FALSE(timer.IsRunning());
    ASSERT_FALSE(timer.user_task().is_null());
    timer.Reset();
    EXPECT_TRUE(timer.IsRunning());
  }
}

TEST(AlarmTimerTest, NonRepeatMessageLoopDeath) {
  timers::AlarmTimer timer(false, false);
  {
    base::MessageLoop loop;
    EXPECT_FALSE(timer.IsRunning());
    timer.Start(FROM_HERE, base::TimeDelta::FromDays(1),
                base::Bind(&base::DoNothing));
    EXPECT_TRUE(timer.IsRunning());
  }
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_TRUE(timer.user_task().is_null());
}

TEST(AlarmTimerTest, RetainRepeatIsRunning) {
  base::MessageLoop loop;
  timers::AlarmTimer timer(FROM_HERE, base::TimeDelta::FromDays(1),
                          base::Bind(&base::DoNothing), true);
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
}

TEST(AlarmTimerTest, RetainNonRepeatIsRunning) {
  base::MessageLoop loop;
  timers::AlarmTimer timer(FROM_HERE, base::TimeDelta::FromDays(1),
                          base::Bind(&base::DoNothing), false);
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
  timer.Reset();
  EXPECT_TRUE(timer.IsRunning());
}

namespace {

bool g_callback_happened1 = false;
bool g_callback_happened2 = false;

void ClearAllCallbackHappened() {
  g_callback_happened1 = false;
  g_callback_happened2 = false;
}

void SetCallbackHappened1() {
  g_callback_happened1 = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

void SetCallbackHappened2() {
  g_callback_happened2 = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

TEST(AlarmTimerTest, ContinuationStopStart) {
  {
    ClearAllCallbackHappened();
    base::MessageLoop loop;
    timers::AlarmTimer timer(false, false);
    timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(10),
                base::Bind(&SetCallbackHappened1));
    timer.Stop();
    timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(40),
                base::Bind(&SetCallbackHappened2));
    base::RunLoop().Run();
    EXPECT_FALSE(g_callback_happened1);
    EXPECT_TRUE(g_callback_happened2);
  }
}

TEST(AlarmTimerTest, ContinuationReset) {
  {
    ClearAllCallbackHappened();
    base::MessageLoop loop;
    timers::AlarmTimer timer(false, false);
    timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(10),
                base::Bind(&SetCallbackHappened1));
    timer.Reset();
    // Since Reset happened before task ran, the user_task must not be cleared:
    ASSERT_FALSE(timer.user_task().is_null());
    base::RunLoop().Run();
    EXPECT_TRUE(g_callback_happened1);
  }
}

}  // namespace


namespace {
void TimerRanCallback(bool* did_run) {
  *did_run = true;

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

void RunTest_OneShotTimerConcurrentResetAndTimerFired(
    base::MessageLoop::Type message_loop_type) {
  base::MessageLoop loop(message_loop_type);

  timers::AlarmTimer timer(false, false);
  bool did_run = false;

  timer.Start(
      FROM_HERE, kTenMilliseconds, base::Bind(&TimerRanCallback, &did_run));

  // Sleep twice as long as the timer to ensure that the timer task gets queued.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));

  // Now reset the timer.  This is attempting to simulate the timer firing and
  // being reset at the same time.  The previously queued task should be
  // removed.
  timer.Reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(did_run);

  // If the previous check failed, running the message loop again will hang the
  // test so we only do it if the callback has not run yet.
  if (!did_run) {
    base::RunLoop().Run();
    EXPECT_TRUE(did_run);
  }
}

void RunTest_RepeatingTimerConcurrentResetAndTimerFired(
    base::MessageLoop::Type message_loop_type) {
  base::MessageLoop loop(message_loop_type);

  timers::AlarmTimer timer(true, true);
  bool did_run = false;

  timer.Start(
      FROM_HERE, kTenMilliseconds, base::Bind(&TimerRanCallback, &did_run));

  // Sleep more that three times as long as the timer duration to ensure that
  // multiple tasks get queued.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(35));

  // Now reset the timer.  This is attempting to simulate a very busy message
  // loop where multiple tasks get queued but the timer gets reset before any of
  // them have a chance to run.
  timer.Reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(did_run);

  // If the previous check failed, running the message loop again will hang the
  // test so we only do it if the callback has not run yet.
  if (!did_run) {
    base::RunLoop().Run();
    EXPECT_TRUE(did_run);
  }
}

TEST(AlarmTimerTest, OneShotTimerConcurrentResetAndTimerFired) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_OneShotTimerConcurrentResetAndTimerFired(testing_message_loops[i]);
  }
}

TEST(AlarmTimerTest, RepeatingTimerConcurrentResetAndTimerFired) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_RepeatingTimerConcurrentResetAndTimerFired(
        testing_message_loops[i]);
  }
}

}  // namespace

}  // namespace timers

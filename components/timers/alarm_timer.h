// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TIMER_ALARM_TIMER_H_
#define COMPONENTS_TIMER_ALARM_TIMER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
struct PendingTask;
}

namespace timers {
// The class implements a timer that is capable of waking the system up from a
// suspended state.  For example, this is useful for running tasks that are
// needed for maintaining network connectivity, like sending heartbeat messages.
// Currently, this feature is only available on Chrome OS systems running linux
// version 3.11 or higher.  On all other platforms, the AlarmTimer behaves
// exactly the same way as a regular Timer.
class AlarmTimer : public base::Timer,
                   public base::MessageLoop::DestructionObserver {
 public:
  // The delegate is responsible for managing the system level details for
  // actually setting up and monitoring a timer that is capable of waking the
  // system from suspend.  This class is reference counted because it may need
  // to outlive the timer in order to clean up after itself.
  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    // Initializes the timer.  Should return true iff the system has timers that
    // can wake it up from suspend.  Will only be called once.
    virtual bool Init(base::WeakPtr<AlarmTimer> timer) = 0;

    // Stops the currently running timer.  It should be safe to call this more
    // than once.
    virtual void Stop() = 0;

    // Resets the timer to fire after |delay| has passed.  Cancels any
    // pre-existing delay.
    virtual void Reset(base::TimeDelta delay) = 0;

   protected:
    virtual ~Delegate() {}

   private:
    friend class base::RefCountedThreadSafe<Delegate>;
  };

  AlarmTimer(bool retain_user_task, bool is_repeating);

  AlarmTimer(const tracked_objects::Location& posted_from,
             base::TimeDelta delay,
             const base::Closure& user_task,
             bool is_repeating);

  ~AlarmTimer() override;

  bool can_wake_from_suspend() const { return can_wake_from_suspend_; }

  // Must be called by the delegate to indicate that the timer has fired and
  // that the user task should be run.
  void OnTimerFired();

  // Timer overrides.
  void Stop() override;
  void Reset() override;

  // MessageLoop::DestructionObserver overrides.
  void WillDestroyCurrentMessageLoop() override;

 private:
  // Initializes the timer with the appropriate delegate.
  void Init();

  // Delegate that will manage actually setting the timer.
  scoped_refptr<Delegate> delegate_;

  // Keeps track of the user task we want to run.  A new one is constructed
  // every time Reset() is called.
  scoped_ptr<base::PendingTask> pending_task_;

  // Tracks whether the timer has the ability to wake the system up from
  // suspend.  This is a runtime check because we won't know if the system
  // supports being woken up from suspend until the delegate actually tries to
  // set it up.
  bool can_wake_from_suspend_;

  // Pointer to the message loop that started the timer.  Used to track the
  // destruction of that message loop.
  base::MessageLoop* origin_message_loop_;

  base::WeakPtrFactory<AlarmTimer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AlarmTimer);
};

}  // namespace timers

#endif  // COMPONENTS_TIMER_ALARM_TIMER_H_

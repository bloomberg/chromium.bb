// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/browser_test_message_pump_android.h"

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"

namespace content {

struct BrowserTestMessagePumpAndroid::RunState {
  RunState(base::MessagePump::Delegate* delegate, int run_depth)
      : delegate(delegate),
        run_depth(run_depth),
        should_quit(false),
        waitable_event(false, false) {
  }

  base::MessagePump::Delegate* delegate;

  // Used to count how many Run() invocations are on the stack.
  int run_depth;

  // Used to flag that the current Run() invocation should return ASAP.
  bool should_quit;

  // Used to sleep until there is more work to do.
  base::WaitableEvent waitable_event;

  // The time at which we should call DoDelayedWork.
  base::TimeTicks delayed_work_time;
};

BrowserTestMessagePumpAndroid::BrowserTestMessagePumpAndroid()
    : state_(NULL) {
}

BrowserTestMessagePumpAndroid::~BrowserTestMessagePumpAndroid() {
}

void BrowserTestMessagePumpAndroid::Run(Delegate* delegate) {
  RunState state(delegate, state_ ? state_->run_depth + 1 : 1);
  RunState* previous_state = state_;
  state_ = &state;
  DCHECK(state_->run_depth <= 1) << "Only one level nested loops supported";

  for (;;) {
    if (state_->should_quit)
      break;

    bool did_work = state_->delegate->DoWork();
    if (state_->should_quit)
      break;

    did_work |= state_->delegate->DoDelayedWork(&state_->delayed_work_time);
    if (state_->should_quit)
      break;

    if (did_work) {
      continue;
    }

    did_work = state_->delegate->DoIdleWork();
    if (state_->should_quit)
      break;

    if (did_work)
      continue;

    if (state_->delayed_work_time.is_null()) {
      state_->waitable_event.Wait();
    } else {
      base::TimeDelta delay =
          state_->delayed_work_time - base::TimeTicks::Now();
      if (delay > base::TimeDelta()) {
        state_->waitable_event.TimedWait(delay);
      } else {
        // It looks like delayed_work_time indicates a time in the past, so we
        // need to call DoDelayedWork now.
        state_->delayed_work_time = base::TimeTicks();
      }
    }
  }

  state_ = previous_state;
}

void BrowserTestMessagePumpAndroid::Quit() {
  if (state_) {
    state_->should_quit = true;
    state_->waitable_event.Signal();
    return;
  }

  base::MessagePumpForUI::Quit();
}

void BrowserTestMessagePumpAndroid::ScheduleWork() {
  if (state_) {
    state_->waitable_event.Signal();
    return;
  }

  base::MessagePumpForUI::ScheduleWork();
}

void BrowserTestMessagePumpAndroid::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  if (state_) {
    // We know that we can't be blocked on Wait right now since this method can
    // only be called on the same thread as Run, so we only need to update our
    // record of how long to sleep when we do sleep.
    state_->delayed_work_time = delayed_work_time;
    return;
  }

  base::MessagePumpForUI::ScheduleDelayedWork(delayed_work_time);
}

}  // namespace base

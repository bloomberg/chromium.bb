// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/status_change_checker.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"

StatusChangeChecker::StatusChangeChecker() : timed_out_(false) {}

StatusChangeChecker::~StatusChangeChecker() {}

bool StatusChangeChecker::Wait() {
  if (IsExitConditionSatisfied()) {
    DVLOG(1) << "Already satisfied: " << GetDebugMessage();
  } else {
    DVLOG(1) << "Blocking: " << GetDebugMessage();
    StartBlockingWait();
  }
  return !TimedOut();
}

bool StatusChangeChecker::TimedOut() const {
  return timed_out_;
}

base::TimeDelta StatusChangeChecker::GetTimeoutDuration() {
  return base::TimeDelta::FromSeconds(45);
}

void StatusChangeChecker::StartBlockingWait() {
  base::OneShotTimer timer;
  timer.Start(FROM_HERE,
              GetTimeoutDuration(),
              base::Bind(&StatusChangeChecker::OnTimeout,
                         base::Unretained(this)));

  {
    base::MessageLoop* loop = base::MessageLoop::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    base::RunLoop().Run();
  }
}

void StatusChangeChecker::StopWaiting() {
  base::MessageLoop::current()->QuitWhenIdle();
}

void StatusChangeChecker::CheckExitCondition() {
  DVLOG(1) << "Await -> Checking Condition: " << GetDebugMessage();
  if (IsExitConditionSatisfied()) {
    DVLOG(1) << "Await -> Condition met: " << GetDebugMessage();
    StopWaiting();
  }
}

void StatusChangeChecker::OnTimeout() {
  DVLOG(1) << "Await -> Timed out: " << GetDebugMessage();
  timed_out_ = true;
  StopWaiting();
}

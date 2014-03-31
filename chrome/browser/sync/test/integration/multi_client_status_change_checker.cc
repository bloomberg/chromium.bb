// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

#include "base/logging.h"
#include "base/scoped_observer.h"
#include "chrome/browser/sync/profile_sync_service.h"

MultiClientStatusChangeChecker::MultiClientStatusChangeChecker(
    std::vector<ProfileSyncService*> services)
  : services_(services), timed_out_(false) {}

MultiClientStatusChangeChecker::~MultiClientStatusChangeChecker() {}

base::TimeDelta MultiClientStatusChangeChecker::GetTimeoutDuration() {
  return base::TimeDelta::FromSeconds(45);
}
void MultiClientStatusChangeChecker::OnTimeout() {
  DVLOG(1) << "Await -> Timed out: " << GetDebugMessage();
  timed_out_ = true;
  base::MessageLoop::current()->QuitWhenIdle();
}

void MultiClientStatusChangeChecker::Wait() {
  DVLOG(1) << "Await: " << GetDebugMessage();

  if (IsExitConditionSatisfied()) {
    DVLOG(1) << "Await -> Exit before waiting: " << GetDebugMessage();
    return;
  }

  ScopedObserver<ProfileSyncService, MultiClientStatusChangeChecker> obs(this);
  for (std::vector<ProfileSyncService*>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    obs.Add(*it);
  }

  base::OneShotTimer<MultiClientStatusChangeChecker> timer;
  timer.Start(FROM_HERE,
              GetTimeoutDuration(),
              base::Bind(&MultiClientStatusChangeChecker::OnTimeout,
                         base::Unretained(this)));

  {
    base::MessageLoop* loop = base::MessageLoop::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    loop->Run();
  }
}

void MultiClientStatusChangeChecker::OnStateChanged() {
  DVLOG(1) << "Await -> Checking Condition: " << GetDebugMessage();
  if (IsExitConditionSatisfied()) {
    DVLOG(1) << "Await -> Condition met: " << GetDebugMessage();
    base::MessageLoop::current()->QuitWhenIdle();
  }
}

bool MultiClientStatusChangeChecker::TimedOut() {
  return timed_out_;
}

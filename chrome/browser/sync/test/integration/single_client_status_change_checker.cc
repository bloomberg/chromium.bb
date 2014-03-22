// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

#include "base/logging.h"
#include "base/scoped_observer.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"

SingleClientStatusChangeChecker::SingleClientStatusChangeChecker(
    ProfileSyncService* service) : service_(service), timed_out_(false) {}

SingleClientStatusChangeChecker::~SingleClientStatusChangeChecker() {}

base::TimeDelta SingleClientStatusChangeChecker::GetTimeoutDuration() {
  return base::TimeDelta::FromSeconds(45);
}
void SingleClientStatusChangeChecker::OnTimeout() {
  DVLOG(1) << "Await -> Timed out: " << GetDebugMessage();
  timed_out_ = true;
  base::MessageLoop::current()->QuitWhenIdle();
}

void SingleClientStatusChangeChecker::Await() {
  DVLOG(1) << "Await: " << GetDebugMessage();

  if (IsExitConditionSatisfied()) {
    DVLOG(1) << "Await -> Exit before waiting: " << GetDebugMessage();
    return;
  }

  ScopedObserver<ProfileSyncService, SingleClientStatusChangeChecker> obs(this);
  obs.Add(service());

  base::OneShotTimer<SingleClientStatusChangeChecker> timer;
  timer.Start(FROM_HERE,
              GetTimeoutDuration(),
              base::Bind(&SingleClientStatusChangeChecker::OnTimeout,
                         base::Unretained(this)));

  {
    base::MessageLoop* loop = base::MessageLoop::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    loop->Run();
  }
}

void SingleClientStatusChangeChecker::OnStateChanged() {
  DVLOG(1) << "Await -> Checking Condition: " << GetDebugMessage();
  if (IsExitConditionSatisfied()) {
    DVLOG(1) << "Await -> Condition met: " << GetDebugMessage();
    base::MessageLoop::current()->QuitWhenIdle();
  }
}

bool SingleClientStatusChangeChecker::TimedOut() {
  return timed_out_;
}

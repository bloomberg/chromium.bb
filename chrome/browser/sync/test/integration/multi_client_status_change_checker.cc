// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

#include "base/logging.h"
#include "base/scoped_observer.h"
#include "chrome/browser/sync/profile_sync_service.h"

MultiClientStatusChangeChecker::MultiClientStatusChangeChecker(
    std::vector<ProfileSyncService*> services)
  : services_(services) {}

MultiClientStatusChangeChecker::~MultiClientStatusChangeChecker() {}

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

  StartBlockingWait();
}

void MultiClientStatusChangeChecker::OnStateChanged() {
  CheckExitCondition();
}

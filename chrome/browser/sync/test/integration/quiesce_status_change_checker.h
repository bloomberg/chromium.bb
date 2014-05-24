// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_QUIESCE_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_QUIESCE_STATUS_CHANGE_CHECKER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"

class ProfileSyncService;
class ProgressMarkerWatcher;

// Waits until all provided clients have finished committing any unsynced items
// and downloading each others' udpates.
//
// This requires that "self-notifications" be enabled.  Otherwise the clients
// will not fetch the latest progress markers on their own, and the latest
// progress markers are needed to confirm that clients are in sync.
//
// There is a race condition here.  If we manage to perform the check at
// precisely the wrong time, we could end up seeing stale snapshot state
// (crbug.com/95742), which would make us think that the client has finished
// syncing when it hasn't.  In practice, this race is rare enough that it
// doesn't cause test failures.
class QuiesceStatusChangeChecker : public StatusChangeChecker {
 public:
  explicit QuiesceStatusChangeChecker(
      std::vector<ProfileSyncService*> services);
  virtual ~QuiesceStatusChangeChecker();

  // Blocks until all clients have quiesced or we time out.
  void Wait();

  // A callback function for some helper objects.
  void OnServiceStateChanged(ProfileSyncService* service);

  // Implementation of StatusChangeChecker.
  virtual bool IsExitConditionSatisfied() OVERRIDE;
  virtual std::string GetDebugMessage() const OVERRIDE;

 private:
  std::vector<ProfileSyncService*> services_;
  ScopedVector<ProgressMarkerWatcher> observers_;

  DISALLOW_COPY_AND_ASSIGN(QuiesceStatusChangeChecker);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_QUIESCE_STATUS_CHANGE_CHECKER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"

#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"

UpdatedProgressMarkerChecker::UpdatedProgressMarkerChecker(
    browser_sync::ProfileSyncService* service)
    : SingleClientStatusChangeChecker(service) {}

bool UpdatedProgressMarkerChecker::IsExitConditionSatisfied() {
  // Checks to see if our self-notify sync cycle has completed and
  // there's nothing to commit.
  //
  // If we assume that no one else is committing at this time and that the
  // current client did not commit anything in its previous sync cycle, then
  // this client has the latest progress markers.
  //
  // The !service()->HasUnsyncedItems() check makes sure that we have nothing to
  // commit.
  //
  // There is a subtle race condition here.  While committing items, the syncer
  // will unset the IS_UNSYNCED bits in the directory.  However, the evidence of
  // this current sync cycle won't be available from GetLastCycleSnapshot()
  // until the sync cycle completes.  If we query this condition between the
  // commit response processing and the end of the sync cycle, we could return a
  // false positive.
  //
  // In practice, this doesn't happen very often because we only query the
  // status when the waiting first starts and when we receive notification of a
  // sync session complete or other significant event from the
  // ProfileSyncService.  If we're calling this right after the sync session
  // completes, then the snapshot is much more likely to be up to date.
  const syncer::SyncCycleSnapshot& snap = service()->GetLastCycleSnapshot();
  return snap.model_neutral_state().num_successful_commits == 0 &&
         service()->IsSyncActive() && !service()->HasUnsyncedItems();
}

std::string UpdatedProgressMarkerChecker::GetDebugMessage() const {
  return "Waiting for progress markers";
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/browser_sync/browser/profile_sync_service.h"

namespace suggestions {

SyncState GetSyncState(Profile* profile) {
  ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (!sync)
    return SyncState::SYNC_OR_HISTORY_SYNC_DISABLED;
  return GetSyncState(
      sync->CanSyncStart(), sync->IsSyncActive() && sync->ConfigurationDone(),
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES));
}

}  // namespace suggestions

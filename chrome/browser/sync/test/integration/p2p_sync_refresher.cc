// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/p2p_sync_refresher.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/browser/notification_service.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

P2PSyncRefresher::P2PSyncRefresher(ProfileSyncService* sync_service)
    : sync_service_(sync_service){
  sync_service_->AddObserver(this);
}

P2PSyncRefresher::~P2PSyncRefresher() {
  sync_service_->RemoveObserver(this);
}

void P2PSyncRefresher::OnStateChanged() {}

void P2PSyncRefresher::OnSyncCycleCompleted() {
  const syncer::sessions::SyncSessionSnapshot& snap =
      sync_service_->GetLastSessionSnapshot();
  bool is_notifiable_commit =
      (snap.model_neutral_state().num_successful_commits > 0);
  if (is_notifiable_commit) {
    syncer::ModelTypeSet model_types =
        snap.model_neutral_state().commit_request_types;
    SyncTest* test = sync_datatype_helper::test();
    for (int i = 0; i < test->num_clients(); ++i) {
      if (sync_service_->profile() != test->GetProfile(i)) {
        content::NotificationService::current()->Notify(
            chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
            content::Source<Profile>(test->GetProfile(i)),
            content::Details<const syncer::ModelTypeSet>(&model_types));
      }
    }
  }
}

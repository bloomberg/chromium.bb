// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/p2p_invalidation_forwarder.h"

#include "chrome/browser/sync/glue/invalidation_helper.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "components/invalidation/p2p_invalidation_service.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

P2PInvalidationForwarder::P2PInvalidationForwarder(
    ProfileSyncService* sync_service,
    invalidation::P2PInvalidationService* invalidation_service)
  : sync_service_(sync_service),
    invalidation_service_(invalidation_service) {
  sync_service_->AddObserver(this);
}

P2PInvalidationForwarder::~P2PInvalidationForwarder() {
  sync_service_->RemoveObserver(this);
}

void P2PInvalidationForwarder::OnStateChanged() {}

void P2PInvalidationForwarder::OnSyncCycleCompleted() {
  const syncer::sessions::SyncSessionSnapshot& snap =
      sync_service_->GetLastSessionSnapshot();
  bool is_notifiable_commit =
      (snap.model_neutral_state().num_successful_commits > 0);
  if (is_notifiable_commit && invalidation_service_) {
    syncer::ModelTypeSet model_types =
        snap.model_neutral_state().commit_request_types;
    syncer::ObjectIdSet ids = ModelTypeSetToObjectIdSet(model_types);
    invalidation_service_->SendInvalidation(ids);
  }
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_service.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/user_events/user_event_sync_bridge.h"

using sync_pb::UserEventSpecifics;

namespace syncer {

UserEventService::UserEventService(SyncService* sync_service,
                                   std::unique_ptr<UserEventSyncBridge> bridge)
    : sync_service_(sync_service),
      bridge_(std::move(bridge)),
      session_id_(base::RandUint64()) {
  // TODO(skym): Subscribe to events about field trial membership changing.
}

UserEventService::~UserEventService() {}

void UserEventService::Shutdown() {}

void UserEventService::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  if (CanRecordEvent(*specifics)) {
    DCHECK(!specifics->has_session_id());
    specifics->set_session_id(session_id_);
    bridge_->RecordUserEvent(std::move(specifics));
  }
}

void UserEventService::RecordUserEvent(const UserEventSpecifics& specifics) {
  RecordUserEvent(base::MakeUnique<UserEventSpecifics>(specifics));
}

base::WeakPtr<ModelTypeSyncBridge> UserEventService::GetSyncBridge() {
  return bridge_->AsWeakPtr();
}

bool UserEventService::CanRecordEvent(const UserEventSpecifics& specifics) {
  // We only record events if the user is syncing history and has not enabled
  // a custom passphrase. The type HISTORY_DELETE_DIRECTIVES is enabled in and
  // only in this exact scenario.
  return base::FeatureList::IsEnabled(switches::kSyncUserEvents) &&
         sync_service_ != nullptr && sync_service_->IsEngineInitialized() &&
         sync_service_->GetPreferredDataTypes().Has(HISTORY_DELETE_DIRECTIVES);
}

void RegisterDependentFieldTrial(const std::string& trial_name,
                                 UserEventSpecifics::EventCase event_case) {
  // TODO(skym): Implementation.
}

}  // namespace syncer

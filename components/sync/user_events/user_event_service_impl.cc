// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/rand_util.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/user_events/user_event_sync_bridge.h"

using sync_pb::UserEventSpecifics;

namespace syncer {

UserEventServiceImpl::UserEventServiceImpl(
    SyncService* sync_service,
    std::unique_ptr<UserEventSyncBridge> bridge)
    : sync_service_(sync_service),
      bridge_(std::move(bridge)),
      session_id_(base::RandUint64()) {
  DCHECK(bridge_);
  DCHECK(sync_service_);
  // TODO(skym): Subscribe to events about field trial membership changing.
}

UserEventServiceImpl::~UserEventServiceImpl() {}

void UserEventServiceImpl::Shutdown() {}

void UserEventServiceImpl::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  if (ShouldRecordEvent(*specifics)) {
    DCHECK(!specifics->has_session_id());
    specifics->set_session_id(session_id_);
    bridge_->RecordUserEvent(std::move(specifics));
  }
}

void UserEventServiceImpl::RecordUserEvent(
    const UserEventSpecifics& specifics) {
  RecordUserEvent(std::make_unique<UserEventSpecifics>(specifics));
}

base::WeakPtr<ModelTypeSyncBridge> UserEventServiceImpl::GetSyncBridge() {
  return bridge_->AsWeakPtr();
}

// static
bool UserEventServiceImpl::MightRecordEvents(bool off_the_record,
                                             SyncService* sync_service) {
  return !off_the_record && sync_service &&
         base::FeatureList::IsEnabled(switches::kSyncUserEvents);
}

bool UserEventServiceImpl::ShouldRecordEvent(
    const UserEventSpecifics& specifics) {
  // We only record events if the user is syncing history (as indicated by
  // GetPreferredDataTypes()) and has not enabled a custom passphrase (as
  // indicated by IsUsingSecondaryPassphrase()).
  return sync_service_->IsEngineInitialized() &&
         !sync_service_->IsUsingSecondaryPassphrase() &&
         sync_service_->GetPreferredDataTypes().Has(HISTORY_DELETE_DIRECTIVES);
}

void UserEventServiceImpl::RegisterDependentFieldTrial(
    const std::string& trial_name,
    UserEventSpecifics::EventCase event_case) {
  // TODO(skym): Implementation.
}

}  // namespace syncer

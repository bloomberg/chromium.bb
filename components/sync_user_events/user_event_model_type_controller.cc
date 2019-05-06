// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/user_event_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

namespace syncer {

UserEventModelTypeController::UserEventModelTypeController(
    SyncService* sync_service,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_on_disk)
    : ModelTypeController(syncer::USER_EVENTS, std::move(delegate_on_disk)),
      sync_service_(sync_service) {
  DCHECK(sync_service_);
  sync_service_->AddObserver(this);
}

UserEventModelTypeController::~UserEventModelTypeController() {
  sync_service_->RemoveObserver(this);
}

bool UserEventModelTypeController::ReadyForStart() const {
  return !sync_service_->GetUserSettings()->IsUsingSecondaryPassphrase();
}

void UserEventModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  // Just disable if we start encrypting; we do not care about re-enabling it
  // during run-time.
  if (ReadyForStart()) {
    return;
  }

  // This results in stopping the data type (and is a no-op if the data type is
  // already stopped because of being unready).
  sync->ReadyForStartChanged(type());
}

}  // namespace syncer

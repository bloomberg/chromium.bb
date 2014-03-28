// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_error_controller.h"

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"

SyncErrorController::SyncErrorController(ProfileSyncService* service)
    : service_(service) {
  DCHECK(service_);
}

SyncErrorController::~SyncErrorController() {
}

bool SyncErrorController::HasError() {
  return service_->HasSyncSetupCompleted() &&
      service_->IsPassphraseRequired() &&
      service_->IsPassphraseRequiredForDecryption();
}

void SyncErrorController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SyncErrorController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SyncErrorController::OnStateChanged() {
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnErrorChanged());
}

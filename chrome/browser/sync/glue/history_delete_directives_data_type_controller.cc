// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/history_delete_directives_data_type_controller.h"

#include "components/sync_driver/sync_client.h"
#include "components/sync_driver/sync_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

HistoryDeleteDirectivesDataTypeController::
    HistoryDeleteDirectivesDataTypeController(
        const base::Closure& error_callback,
        sync_driver::SyncClient* sync_client)
    : sync_driver::UIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          error_callback,
          syncer::HISTORY_DELETE_DIRECTIVES,
          sync_client),
      sync_client_(sync_client) {}

HistoryDeleteDirectivesDataTypeController::
    ~HistoryDeleteDirectivesDataTypeController() {
}

bool HistoryDeleteDirectivesDataTypeController::ReadyForStart() const {
  return !sync_client_->GetSyncService()->IsEncryptEverythingEnabled();
}

bool HistoryDeleteDirectivesDataTypeController::StartModels() {
  if (DisableTypeIfNecessary())
    return false;
  sync_client_->GetSyncService()->AddObserver(this);
  return true;
}

void HistoryDeleteDirectivesDataTypeController::StopModels() {
  if (sync_client_->GetSyncService()->HasObserver(this))
    sync_client_->GetSyncService()->RemoveObserver(this);
}

void HistoryDeleteDirectivesDataTypeController::OnStateChanged() {
  DisableTypeIfNecessary();
}

bool HistoryDeleteDirectivesDataTypeController::DisableTypeIfNecessary() {
  if (!sync_client_->GetSyncService()->IsSyncActive())
    return false;

  if (ReadyForStart())
    return false;

  if (sync_client_->GetSyncService()->HasObserver(this))
    sync_client_->GetSyncService()->RemoveObserver(this);
  syncer::SyncError error(
      FROM_HERE,
      syncer::SyncError::DATATYPE_POLICY_ERROR,
      "Delete directives not supported with encryption.",
      type());
  OnSingleDataTypeUnrecoverableError(error);
  return true;
}

}  // namespace browser_sync

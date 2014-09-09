// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/history_delete_directives_data_type_controller.h"

#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

HistoryDeleteDirectivesDataTypeController::
    HistoryDeleteDirectivesDataTypeController(
        sync_driver::SyncApiComponentFactory* factory,
        ProfileSyncService* sync_service)
    : sync_driver::UIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          syncer::HISTORY_DELETE_DIRECTIVES,
          factory),
      sync_service_(sync_service) {
}

HistoryDeleteDirectivesDataTypeController::
    ~HistoryDeleteDirectivesDataTypeController() {
}

bool HistoryDeleteDirectivesDataTypeController::ReadyForStart() const {
  return !sync_service_->EncryptEverythingEnabled();
}

bool HistoryDeleteDirectivesDataTypeController::StartModels() {
  if (DisableTypeIfNecessary())
    return false;
  sync_service_->AddObserver(this);
  return true;
}

void HistoryDeleteDirectivesDataTypeController::StopModels() {
  if (sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
}

void HistoryDeleteDirectivesDataTypeController::OnStateChanged() {
  DisableTypeIfNecessary();
}

bool HistoryDeleteDirectivesDataTypeController::DisableTypeIfNecessary() {
  if (!sync_service_->ShouldPushChanges())
    return false;

  if (ReadyForStart())
    return false;

  if (sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
  syncer::SyncError error(
      FROM_HERE,
      syncer::SyncError::DATATYPE_POLICY_ERROR,
      "Delete directives not supported with encryption.",
      type());
  OnSingleDataTypeUnrecoverableError(error);
  return true;
}

}  // namespace browser_sync

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
  sync_service_->AddObserver(this);
}

HistoryDeleteDirectivesDataTypeController::
    ~HistoryDeleteDirectivesDataTypeController() {
  sync_service_->RemoveObserver(this);
}

bool HistoryDeleteDirectivesDataTypeController::ReadyForStart() const {
  return !sync_service_->EncryptEverythingEnabled();
}

void HistoryDeleteDirectivesDataTypeController::OnStateChanged() {
  if ((state() != NOT_RUNNING || state() != STOPPING) &&
      sync_service_->ShouldPushChanges() && !ReadyForStart()) {
    syncer::SyncError error(
        FROM_HERE,
        syncer::SyncError::DATATYPE_POLICY_ERROR,
        "Delete directives not supported with encryption.",
        type());
    OnSingleDataTypeUnrecoverableError(error);
  }
}

}  // namespace browser_sync

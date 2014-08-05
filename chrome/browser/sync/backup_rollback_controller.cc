// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/backup_rollback_controller.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "components/sync_driver/sync_prefs.h"

namespace browser_sync {

#if defined(ENABLE_PRE_SYNC_BACKUP)
// Number of rollback attempts to try before giving up.
static const int kRollbackLimits = 3;

// Finch experiment name and group.
static char kSyncBackupFinchName[] = "SyncBackup";
static char kSyncBackupFinchDisabled[] = "disabled";
#endif

BackupRollbackController::BackupRollbackController(
    sync_driver::SyncPrefs* sync_prefs,
    const SupervisedUserSigninManagerWrapper* signin,
    base::Closure start_backup,
    base::Closure start_rollback)
    : sync_prefs_(sync_prefs),
      signin_(signin),
      start_backup_(start_backup),
      start_rollback_(start_rollback),
      weak_ptr_factory_(this) {
}

BackupRollbackController::~BackupRollbackController() {
}

void BackupRollbackController::Start(base::TimeDelta delay) {
  if (!IsBackupEnabled())
    return;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSyncDisableRollback)) {
    sync_prefs_->SetRemainingRollbackTries(0);
  }

  if (delay == base::TimeDelta()) {
    TryStart();
  } else {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BackupRollbackController::TryStart,
                   weak_ptr_factory_.GetWeakPtr()),
        delay);
  }
}

void BackupRollbackController::OnRollbackReceived() {
#if defined(ENABLE_PRE_SYNC_BACKUP)
  sync_prefs_->SetRemainingRollbackTries(kRollbackLimits);
#endif
}

void BackupRollbackController::OnRollbackDone() {
#if defined(ENABLE_PRE_SYNC_BACKUP)
  sync_prefs_->SetRemainingRollbackTries(0);
#endif
}

void BackupRollbackController::TryStart() {
  if (!signin_->GetEffectiveUsername().empty()) {
    DVLOG(1) << "Don't start backup/rollback when user is signed in.";
    return;
  }

  int rollback_tries = sync_prefs_->GetRemainingRollbackTries();
  if (rollback_tries > 0) {
    DVLOG(1) << "Start rollback.";
    sync_prefs_->SetRemainingRollbackTries(rollback_tries - 1);
    start_rollback_.Run();
  } else {
    DVLOG(1) << "Start backup.";
    start_backup_.Run();
  }
}

// static
bool BackupRollbackController::IsBackupEnabled() {
#if defined(ENABLE_PRE_SYNC_BACKUP)
  const std::string group_name =
      base::FieldTrialList::FindFullName(kSyncBackupFinchName);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSyncDisableBackup) ||
      group_name == kSyncBackupFinchDisabled)  {
    return false;
  }
  return true;
#else
  return false;
#endif
}

}  // namespace browser_sync

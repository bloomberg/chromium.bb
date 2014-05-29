// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/backup_rollback_controller.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/sync/managed_user_signin_manager_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "components/sync_driver/sync_prefs.h"

namespace browser_sync {

#if defined(OS_WIN) || defined(OS_MACOSX) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
// Number of rollback attempts to try before giving up.
static const int kRollbackLimits = 3;
#endif

BackupRollbackController::BackupRollbackController(
    sync_driver::SyncPrefs* sync_prefs,
    const ManagedUserSigninManagerWrapper* signin,
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
#if defined(OS_WIN) || defined(OS_MACOSX) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSyncEnableBackupRollback)) {
    return;
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
#endif
}

void BackupRollbackController::OnRollbackReceived() {
#if defined(OS_WIN) || defined(OS_MACOSX) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  sync_prefs_->SetRemainingRollbackTries(kRollbackLimits);
#endif
}

void BackupRollbackController::OnRollbackDone() {
#if defined(OS_WIN) || defined(OS_MACOSX) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
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

}  // namespace browser_sync

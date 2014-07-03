// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_BACKUP_ROLLBACK_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_BACKUP_ROLLBACK_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

class SupervisedUserSigninManagerWrapper;

namespace sync_driver {
class SyncPrefs;
}

namespace browser_sync {

// BackupRollbackController takes two closures for starting backup/rollback
// process. It calls the closures according to user's signin status or
// received rollback command. Backup is not run when user signed in, even when
// sync is not running.
class BackupRollbackController {
 public:
  BackupRollbackController(sync_driver::SyncPrefs* sync_prefs,
                           const SupervisedUserSigninManagerWrapper* signin,
                           base::Closure start_backup,
                           base::Closure start_rollback);
  ~BackupRollbackController();

  // Check to see whether to start backup/rollback. |delay| specifies the time
  // to wait before checking.
  void Start(base::TimeDelta delay);

  // Update rollback preference to indicate rollback is needed.
  void OnRollbackReceived();

  // Update rollback preference to indicate rollback is finished.
  void OnRollbackDone();

  // Return true if platform supports backup and backup is enabled.
  static bool IsBackupEnabled();

 private:
  // Check signin status and rollback preference and start backup/rollback
  // accordingly.
  void TryStart();

  sync_driver::SyncPrefs* sync_prefs_;

  // Use SupervisedUserSigninManagerWrapper instead of SigninManagerBase because
  // SigninManagerBase could return non-empty user name for supervised user,
  // which would cause backup to trumpet normal sync for supervised user.
  const SupervisedUserSigninManagerWrapper* signin_;

  base::Closure start_backup_;
  base::Closure start_rollback_;
  base::WeakPtrFactory<BackupRollbackController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackupRollbackController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_BACKUP_ROLLBACK_CONTROLLER_H_

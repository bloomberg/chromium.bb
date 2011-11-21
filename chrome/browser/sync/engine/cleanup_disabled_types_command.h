// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_CLEANUP_DISABLED_TYPES_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_CLEANUP_DISABLED_TYPES_COMMAND_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/sync/engine/syncer_command.h"

namespace browser_sync {

// A syncer command that purges (from memory and disk) entries belonging to
// a ModelType or ServerModelType that the user has not elected to sync.
//
// This is done as part of a session to 1) ensure it does not block the UI,
// and 2) avoid complicated races that could arise between a) deleting
// things b) a sync session trying to use these things c) and the potential
// re-enabling of the data type by the user before some scheduled deletion
// took place.  Here, we are safe to perform I/O synchronously and we know it
// is a safe time to delete as we are in the only active session.
//
// The removal from memory is done synchronously, while the disk purge is left
// to an asynchronous SaveChanges operation.  However, all the updates for
// meta data fields (such as initial_sync_ended) as well as the actual entry
// deletions will be committed in a single sqlite transaction.  Thus it is
// possible that disabled types re-appear (in the sync db) after a reboot,
// but things will remain in a consistent state.  This kind of error case is
// cared for in this command by retrying; see ExecuteImpl.
class CleanupDisabledTypesCommand : public SyncerCommand {
 public:
  CleanupDisabledTypesCommand();
  virtual ~CleanupDisabledTypesCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CleanupDisabledTypesCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_CLEANUP_DISABLED_TYPES_COMMAND_H_


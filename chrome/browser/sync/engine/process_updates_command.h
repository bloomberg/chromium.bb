// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_PROCESS_UPDATES_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_PROCESS_UPDATES_COMMAND_H_
#pragma once

#include "chrome/browser/sync/engine/model_changing_syncer_command.h"
#include "chrome/browser/sync/engine/syncer_types.h"

namespace syncable {
class ScopedDirLookup;
class WriteTransaction;
}

namespace sync_pb {
class SyncEntity;
}

namespace browser_sync {

// A syncer command for processing updates.
//
// Preconditions - updates in the SyncerSesssion have been downloaded
//                 and verified.
//
// Postconditions - All of the verified SyncEntity data will be copied to
//                  the server fields of the corresponding syncable entries.
// TODO(tim): This should not be ModelChanging (bug 36592).
class ProcessUpdatesCommand : public ModelChangingSyncerCommand {
 public:
  ProcessUpdatesCommand();
  virtual ~ProcessUpdatesCommand();

  // ModelChangingSyncerCommand implementation.
  virtual void ModelChangingExecuteImpl(sessions::SyncSession* session);

 private:
  ServerUpdateProcessingResult ProcessUpdate(
      const syncable::ScopedDirLookup& dir,
      const sync_pb::SyncEntity& proto_update,
      syncable::WriteTransaction* const trans);
  DISALLOW_COPY_AND_ASSIGN(ProcessUpdatesCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_PROCESS_UPDATES_COMMAND_H_

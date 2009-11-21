// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_VERIFY_UPDATES_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_VERIFY_UPDATES_COMMAND_H_

#include "base/basictypes.h"

#include "chrome/browser/sync/engine/syncer_command.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/engine/syncer_types.h"

namespace syncable {
class WriteTransaction;
}

namespace browser_sync {

// Verifies the response from a GetUpdates request. All invalid updates will be
// noted in the SyncSession after this command is executed.
class VerifyUpdatesCommand : public SyncerCommand {
 public:
  VerifyUpdatesCommand();
  virtual ~VerifyUpdatesCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session);

  VerifyResult VerifyUpdate(syncable::WriteTransaction* trans,
                            const SyncEntity& entry);
 private:
  DISALLOW_COPY_AND_ASSIGN(VerifyUpdatesCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_VERIFY_UPDATES_COMMAND_H_

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_END_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_END_COMMAND_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer_command.h"

namespace browser_sync {

// A syncer command for wrapping up a sync cycle.
//
// Preconditions - syncing is complete.
//
// Postconditions - The UI has been told that we're done syncing.

class SyncerEndCommand : public SyncerCommand {
 public:
  SyncerEndCommand();
  virtual ~SyncerEndCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session);
 private:
  DISALLOW_COPY_AND_ASSIGN(SyncerEndCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_END_COMMAND_H_

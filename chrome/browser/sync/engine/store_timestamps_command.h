// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_STORE_TIMESTAMPS_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_STORE_TIMESTAMPS_COMMAND_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/sync/engine/syncer_command.h"
#include "chrome/browser/sync/engine/syncer_types.h"

namespace browser_sync {

// A syncer command that extracts the changelog timestamp information from
// a GetUpdatesResponse (fetched in DownloadUpdatesCommand) and stores
// it in the directory.  This is meant to run immediately after
// ProcessUpdatesCommand.
//
// Preconditions - all updates in the SyncerSesssion have been stored in the
//                 database, meaning it is safe to update the persisted
//                 timestamps.
//
// Postconditions - The next_timestamp returned by the server will be
//                  saved into the directory (where it will be used
//                  the next time that DownloadUpdatesCommand runs).
class StoreTimestampsCommand : public SyncerCommand {
 public:
  StoreTimestampsCommand();
  virtual ~StoreTimestampsCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(StoreTimestampsCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_STORE_TIMESTAMPS_COMMAND_H_

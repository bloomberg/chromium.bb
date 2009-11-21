// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_DOWNLOAD_UPDATES_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_DOWNLOAD_UPDATES_COMMAND_H_

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer_command.h"

namespace browser_sync {

// Downloads updates from the server and places them in the SyncSession.
class DownloadUpdatesCommand : public SyncerCommand {
 public:
  DownloadUpdatesCommand();
  virtual ~DownloadUpdatesCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session);

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadUpdatesCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_DOWNLOAD_UPDATES_COMMAND_H_

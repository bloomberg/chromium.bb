// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_

#include "base/basictypes.h"
#include "chrome/browser/sync/engine/syncer_command.h"
#include "chrome/browser/sync/engine/syncer_session.h"

namespace browser_sync {

class BuildCommitCommand : public SyncerCommand {
 public:
  BuildCommitCommand();
  virtual ~BuildCommitCommand();

  virtual void ExecuteImpl(SyncerSession *session);

 private:
  void AddExtensionsActivityToMessage(SyncerSession* session,
                                      CommitMessage* message);
  DISALLOW_COPY_AND_ASSIGN(BuildCommitCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_POST_COMMIT_MESSAGE_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_POST_COMMIT_MESSAGE_COMMAND_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/sync/engine/syncer_command.h"

namespace browser_sync {

class PostCommitMessageCommand : public SyncerCommand {
 public:
  PostCommitMessageCommand();
  virtual ~PostCommitMessageCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PostCommitMessageCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_POST_COMMIT_MESSAGE_COMMAND_H_

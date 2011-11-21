// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_
#define CHROME_BROWSER_SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/engine/syncer_command.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

class BuildCommitCommand : public SyncerCommand {
 public:
  BuildCommitCommand();
  virtual ~BuildCommitCommand();

  // SyncerCommand implementation.
  virtual void ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(BuildCommitCommandTest, InterpolatePosition);

  // Functions returning constants controlling range of values.
  static int64 GetFirstPosition();
  static int64 GetLastPosition();
  static int64 GetGap();

  void AddExtensionsActivityToMessage(sessions::SyncSession* session,
                                      CommitMessage* message);
  // Helper for computing position.  Find the numeric position value
  // of the closest already-synced entry.  |direction| must be one of
  // NEXT_ID or PREV_ID; this parameter controls the search direction.
  // For an open range (no predecessor or successor), the return
  // value will be kFirstPosition or kLastPosition.
  int64 FindAnchorPosition(syncable::IdField direction,
                           const syncable::Entry& entry);
  // Given two values of the type returned by FindAnchorPosition,
  // compute a third value in between the two ranges.
  int64 InterpolatePosition(int64 lo, int64 hi);

  DISALLOW_COPY_AND_ASSIGN(BuildCommitCommand);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_BUILD_COMMIT_COMMAND_H_

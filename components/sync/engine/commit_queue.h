// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_COMMIT_QUEUE_H_
#define COMPONENTS_SYNC_ENGINE_COMMIT_QUEUE_H_

#include "components/sync/engine/non_blocking_sync_common.h"

namespace syncer {

// Interface used by a synced data type to issue requests to the sync backend.
class CommitQueue {
 public:
  CommitQueue();
  virtual ~CommitQueue();

  // Nudge sync engine to indicate the datatype has local changes to commit.
  virtual void NudgeForCommit() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_COMMIT_QUEUE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_QUEUE_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_QUEUE_H_

#include "components/sync/core/non_blocking_sync_common.h"

namespace syncer_v2 {

// Interface used by a synced data type to issue requests to the sync backend.
class SYNC_EXPORT CommitQueue {
 public:
  CommitQueue();
  virtual ~CommitQueue();

  // Entry point for the ModelTypeProcessor to send commit requests.
  virtual void EnqueueForCommit(const CommitRequestDataList& list) = 0;
};

}  // namespace syncer_v2

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_QUEUE_H_

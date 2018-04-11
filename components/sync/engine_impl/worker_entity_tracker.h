// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_WORKER_ENTITY_TRACKER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_WORKER_ENTITY_TRACKER_H_

#include <memory>

#include "components/sync/engine/non_blocking_sync_common.h"

namespace syncer {

// Manages the pending commit and update state for an entity on the sync
// thread.
//
// It should be considered a helper class internal to the
// ModelTypeWorker.
//
// Maintains the state associated with a particular sync entity which is
// necessary for decision-making on the sync thread. It can track pending
// commit state, received update state, and can detect conflicts.
//
// This object may contain state associated with a pending commit, pending
// update, or both.
class WorkerEntityTracker {
 public:
  WorkerEntityTracker();
  ~WorkerEntityTracker();

  // Handles the receipt of an encrypted update from the server.
  void ReceiveEncryptedUpdate(const UpdateResponseData& data);

  // Functions to fetch the latest encrypted update.
  bool HasEncryptedUpdate() const;
  UpdateResponseData GetEncryptedUpdate() const;

  // Clears the encrypted update. Allows us to resume regular commit behavior.
  void ClearEncryptedUpdate();

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  // An update for this entity which can't be applied right now. The presence
  // of an pending update prevents commits. As of this writing, the only
  // source of pending updates is updates that can't currently be decrypted.
  std::unique_ptr<UpdateResponseData> encrypted_update_;

  DISALLOW_COPY_AND_ASSIGN(WorkerEntityTracker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_WORKER_ENTITY_TRACKER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "gpu/gpu_export.h"

namespace gpu {

// This class manages the sync points, which allow cross-channel
// synchronization.
class GPU_EXPORT SyncPointManager {
 public:
  explicit SyncPointManager(bool allow_threaded_wait);
  ~SyncPointManager();

  // Generates a sync point, returning its ID. This can me called on any thread.
  // IDs start at a random number. Never return 0.
  uint32 GenerateSyncPoint();

  // Retires a sync point. This will call all the registered callbacks for this
  // sync point. This can only be called on the main thread.
  void RetireSyncPoint(uint32 sync_point);

  // Adds a callback to the sync point. The callback will be called when the
  // sync point is retired, or immediately (from within that function) if the
  // sync point was already retired (or not created yet). This can only be
  // called on the main thread.
  void AddSyncPointCallback(uint32 sync_point, const base::Closure& callback);

  bool IsSyncPointRetired(uint32 sync_point);

  // Block and wait until a sync point is signaled. This is only useful when
  // the sync point is signaled on another thread.
  void WaitSyncPoint(uint32 sync_point);

 private:
  typedef std::vector<base::Closure> ClosureList;
  typedef base::hash_map<uint32, ClosureList> SyncPointMap;


  bool IsSyncPointRetiredLocked(uint32 sync_point);

  const bool allow_threaded_wait_;

  // Protects the 2 fields below. Note: callbacks shouldn't be called with this
  // held.
  base::Lock lock_;
  SyncPointMap sync_point_map_;
  uint32 next_sync_point_;
  base::ConditionVariable retire_cond_var_;

  DISALLOW_COPY_AND_ASSIGN(SyncPointManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_

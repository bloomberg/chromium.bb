// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/sync_point_manager.h"

#include <climits>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"

namespace gpu {

static const int kMaxSyncBase = INT_MAX;

SyncPointManager::SyncPointManager(bool allow_threaded_wait)
    : allow_threaded_wait_(allow_threaded_wait),
      // To reduce the risk that a sync point created in a previous GPU process
      // will be in flight in the next GPU process, randomize the starting sync
      // point number. http://crbug.com/373452
      next_sync_point_(base::RandInt(1, kMaxSyncBase)),
      retire_cond_var_(&lock_) {}

SyncPointManager::~SyncPointManager() {}

uint32 SyncPointManager::GenerateSyncPoint() {
  base::AutoLock lock(lock_);
  uint32 sync_point = next_sync_point_++;
  // When an integer overflow occurs, don't return 0.
  if (!sync_point)
    sync_point = next_sync_point_++;

  // Note: wrapping would take days for a buggy/compromized renderer that would
  // insert sync points in a loop, but if that were to happen, better explicitly
  // crash the GPU process than risk worse.
  // For normal operation (at most a few per frame), it would take ~a year to
  // wrap.
  CHECK(sync_point_map_.find(sync_point) == sync_point_map_.end());
  sync_point_map_.insert(std::make_pair(sync_point, ClosureList()));
  return sync_point;
}

void SyncPointManager::RetireSyncPoint(uint32 sync_point) {
  ClosureList list;
  {
    base::AutoLock lock(lock_);
    SyncPointMap::iterator it = sync_point_map_.find(sync_point);
    if (it == sync_point_map_.end()) {
      LOG(ERROR) << "Attempted to retire sync point that"
                    " didn't exist or was already retired.";
      return;
    }
    list.swap(it->second);
    sync_point_map_.erase(it);
    if (allow_threaded_wait_)
      retire_cond_var_.Broadcast();
  }
  for (ClosureList::iterator i = list.begin(); i != list.end(); ++i)
    i->Run();
}

void SyncPointManager::AddSyncPointCallback(uint32 sync_point,
                                            const base::Closure& callback) {
  {
    base::AutoLock lock(lock_);
    SyncPointMap::iterator it = sync_point_map_.find(sync_point);
    if (it != sync_point_map_.end()) {
      it->second.push_back(callback);
      return;
    }
  }
  callback.Run();
}

bool SyncPointManager::IsSyncPointRetired(uint32 sync_point) {
  base::AutoLock lock(lock_);
  return IsSyncPointRetiredLocked(sync_point);
}

void SyncPointManager::WaitSyncPoint(uint32 sync_point) {
  if (!allow_threaded_wait_) {
    DCHECK(IsSyncPointRetired(sync_point));
    return;
  }

  base::AutoLock lock(lock_);
  while (!IsSyncPointRetiredLocked(sync_point)) {
    retire_cond_var_.Wait();
  }
}

bool SyncPointManager::IsSyncPointRetiredLocked(uint32 sync_point) {
  lock_.AssertAcquired();
  return sync_point_map_.find(sync_point) == sync_point_map_.end();
}

}  // namespace gpu

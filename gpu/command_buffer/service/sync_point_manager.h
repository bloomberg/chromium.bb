// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_

#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

namespace gpu {

class SyncPointClient;
class SyncPointManager;

class GPU_EXPORT SyncPointClientState
    : public base::RefCountedThreadSafe<SyncPointClientState> {
 public:
  static scoped_refptr<SyncPointClientState> Create();
  uint32_t GenerateUnprocessedOrderNumber(SyncPointManager* sync_point_manager);

  void BeginProcessingOrderNumber(uint32_t order_num) {
    DCHECK(processing_thread_checker_.CalledOnValidThread());
    DCHECK_GE(order_num, current_order_num_);
    current_order_num_ = order_num;
  }

  void FinishProcessingOrderNumber(uint32_t order_num) {
    DCHECK(processing_thread_checker_.CalledOnValidThread());
    DCHECK_EQ(current_order_num_, order_num);
    DCHECK_GT(order_num, processed_order_num());
    base::subtle::Release_Store(&processed_order_num_, order_num);
  }

  uint32_t processed_order_num() const {
    return base::subtle::Acquire_Load(&processed_order_num_);
  }

  uint32_t unprocessed_order_num() const {
    return base::subtle::Acquire_Load(&unprocessed_order_num_);
  }

  uint32_t current_order_num() const {
    DCHECK(processing_thread_checker_.CalledOnValidThread());
    return current_order_num_;
  }

 protected:
  friend class base::RefCountedThreadSafe<SyncPointClientState>;
  friend class SyncPointClient;

  SyncPointClientState();
  virtual ~SyncPointClientState();

  // Last finished IPC order number.
  base::subtle::Atomic32 processed_order_num_;

  // Unprocessed order number expected to be processed under normal execution.
  base::subtle::Atomic32 unprocessed_order_num_;

  // Non thread-safe functions need to be called from a single thread.
  base::ThreadChecker processing_thread_checker_;

  // Current IPC order number being processed (only used on processing thread).
  uint32_t current_order_num_;

  DISALLOW_COPY_AND_ASSIGN(SyncPointClientState);
};

class GPU_EXPORT SyncPointClient {
 public:
  ~SyncPointClient();

  scoped_refptr<SyncPointClientState> client_state() { return client_state_; }

 private:
  friend class SyncPointManager;

  SyncPointClient(SyncPointManager* sync_point_manager,
                  scoped_refptr<SyncPointClientState> state,
                  CommandBufferNamespace namespace_id, uint64_t client_id);

  // Sync point manager is guaranteed to exist in the lifetime of the client.
  SyncPointManager* sync_point_manager_;

  // Keep the state that is sharable across multiple threads.
  scoped_refptr<SyncPointClientState> client_state_;

  // Unique namespace/client id pair for this sync point client.
  CommandBufferNamespace namespace_id_;
  uint64_t client_id_;

  DISALLOW_COPY_AND_ASSIGN(SyncPointClient);
};

// This class manages the sync points, which allow cross-channel
// synchronization.
class GPU_EXPORT SyncPointManager {
 public:
  explicit SyncPointManager(bool allow_threaded_wait);
  ~SyncPointManager();

  // Creates/Destroy a sync point client which message processors should hold.
  scoped_ptr<SyncPointClient> CreateSyncPointClient(
      scoped_refptr<SyncPointClientState> client_state,
      CommandBufferNamespace namespace_id, uint64_t client_id);

  // Finds the state of an already created sync point client.
  scoped_refptr<SyncPointClientState> GetSyncPointClientState(
    CommandBufferNamespace namespace_id, uint64_t client_id);

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
  friend class SyncPointClient;
  friend class SyncPointClientState;

  typedef std::vector<base::Closure> ClosureList;
  typedef base::hash_map<uint32, ClosureList> SyncPointMap;
  typedef base::hash_map<uint64_t, SyncPointClient*> ClientMap;

  bool IsSyncPointRetiredLocked(uint32 sync_point);
  uint32_t GenerateOrderNumber();
  void DestroySyncPointClient(CommandBufferNamespace namespace_id,
                              uint64_t client_id);

  const bool allow_threaded_wait_;

  // Order number is global for all clients.
  base::AtomicSequenceNumber global_order_num_;

  // Client map holds a map of clients id to client for each namespace.
  base::Lock client_maps_lock_;
  ClientMap client_maps_[NUM_COMMAND_BUFFER_NAMESPACES];

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

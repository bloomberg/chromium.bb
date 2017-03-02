// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/gpu_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {

class SyncPointClient;
class SyncPointClientState;
class SyncPointManager;

class GPU_EXPORT SyncPointOrderData
    : public base::RefCountedThreadSafe<SyncPointOrderData> {
 public:
  static scoped_refptr<SyncPointOrderData> Create();
  void Destroy();

  uint32_t GenerateUnprocessedOrderNumber(SyncPointManager* sync_point_manager);
  void BeginProcessingOrderNumber(uint32_t order_num);
  void PauseProcessingOrderNumber(uint32_t order_num);
  void FinishProcessingOrderNumber(uint32_t order_num);

  uint32_t processed_order_num() const {
    base::AutoLock auto_lock(lock_);
    return processed_order_num_;
  }

  uint32_t unprocessed_order_num() const {
    base::AutoLock auto_lock(lock_);
    return unprocessed_order_num_;
  }

  uint32_t current_order_num() const {
    DCHECK(processing_thread_checker_.CalledOnValidThread());
    return current_order_num_;
  }

  bool IsProcessingOrderNumber() {
    DCHECK(processing_thread_checker_.CalledOnValidThread());
    return !paused_ && current_order_num_ > processed_order_num();
  }

  bool ValidateReleaseOrderNumber(
      scoped_refptr<SyncPointClientState> client_state,
      uint32_t wait_order_num,
      uint64_t fence_release,
      const base::Closure& release_callback);

 private:
  friend class base::RefCountedThreadSafe<SyncPointOrderData>;

  struct OrderFence {
    uint32_t order_num;
    uint64_t fence_release;
    base::Closure release_callback;
    scoped_refptr<SyncPointClientState> client_state;

    OrderFence(uint32_t order,
               uint64_t release,
               const base::Closure& release_callback,
               scoped_refptr<SyncPointClientState> state);
    OrderFence(const OrderFence& other);
    ~OrderFence();

    bool operator>(const OrderFence& rhs) const {
      return std::tie(order_num, fence_release) >
             std::tie(rhs.order_num, rhs.fence_release);
    }
  };
  typedef std::priority_queue<OrderFence,
                              std::vector<OrderFence>,
                              std::greater<OrderFence>>
      OrderFenceQueue;

  SyncPointOrderData();
  ~SyncPointOrderData();

  // Non thread-safe functions need to be called from a single thread.
  base::ThreadChecker processing_thread_checker_;

  // Current IPC order number being processed (only used on processing thread).
  uint32_t current_order_num_ = 0;

  // Whether or not the current order number is being processed or paused.
  bool paused_ = false;

  // This lock protects destroyed_, processed_order_num_,
  // unprocessed_order_num_, and order_fence_queue_. All order numbers (n) in
  // order_fence_queue_ must follow the invariant:
  //   processed_order_num_ < n <= unprocessed_order_num_.
  mutable base::Lock lock_;

  bool destroyed_ = false;

  // Last finished IPC order number.
  uint32_t processed_order_num_ = 0;

  // Unprocessed order number expected to be processed under normal execution.
  uint32_t unprocessed_order_num_ = 0;

  // In situations where we are waiting on fence syncs that do not exist, we
  // validate by making sure the order number does not pass the order number
  // which the wait command was issued. If the order number reaches the
  // wait command's, we should automatically release up to the expected
  // release count. Note that this also releases other lower release counts,
  // so a single misbehaved fence sync is enough to invalidate/signal all
  // previous fence syncs.
  OrderFenceQueue order_fence_queue_;

  DISALLOW_COPY_AND_ASSIGN(SyncPointOrderData);
};

// Internal state for sync point clients.
class GPU_EXPORT SyncPointClientState
    : public base::RefCountedThreadSafe<SyncPointClientState> {
 public:
  explicit SyncPointClientState(scoped_refptr<SyncPointOrderData> order_data);

  bool IsFenceSyncReleased(uint64_t release);

  // Queues the callback to be called if the release is valid. If the release
  // is invalid this function will return False and the callback will never
  // be called.
  bool WaitForRelease(uint64_t release,
                      uint32_t wait_order_num,
                      const base::Closure& callback);

  // Releases a fence sync and all fence syncs below.
  void ReleaseFenceSync(uint64_t release);

  // Does not release the fence sync, but releases callbacks waiting on that
  // fence sync.
  void EnsureWaitReleased(uint64_t release, const base::Closure& callback);

 private:
  friend class base::RefCountedThreadSafe<SyncPointClientState>;

  struct ReleaseCallback {
    uint64_t release_count;
    base::Closure callback_closure;

    ReleaseCallback(uint64_t release, const base::Closure& callback);
    ReleaseCallback(const ReleaseCallback& other);
    ~ReleaseCallback();

    bool operator>(const ReleaseCallback& rhs) const {
      return release_count > rhs.release_count;
    }
  };
  typedef std::priority_queue<ReleaseCallback,
                              std::vector<ReleaseCallback>,
                              std::greater<ReleaseCallback>>
      ReleaseCallbackQueue;

  ~SyncPointClientState();

  // Global order data where releases will originate from.
  scoped_refptr<SyncPointOrderData> order_data_;

  // Protects fence_sync_release_, fence_callback_queue_.
  base::Lock fence_sync_lock_;

  // Current fence sync release that has been signaled.
  uint64_t fence_sync_release_ = 0;

  // In well defined fence sync operations, fence syncs are released in order
  // so simply having a priority queue for callbacks is enough.
  ReleaseCallbackQueue release_callback_queue_;

  DISALLOW_COPY_AND_ASSIGN(SyncPointClientState);
};

class GPU_EXPORT SyncPointClient {
 public:
  SyncPointClient(SyncPointManager* sync_point_manager,
                  scoped_refptr<SyncPointOrderData> order_data,
                  CommandBufferNamespace namespace_id,
                  CommandBufferId command_buffer_id);
  ~SyncPointClient();

  // This behaves similarly to SyncPointManager::Wait but uses the order data
  // to guarantee no deadlocks with other clients.
  bool Wait(const SyncToken& sync_token, const base::Closure& callback);

  // Like Wait but runs the callback on the given task runner's thread.
  bool WaitNonThreadSafe(
      const SyncToken& sync_token,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::Closure& callback);

  // Release fence sync and run queued callbacks.
  void ReleaseFenceSync(uint64_t release);

 private:
  // Sync point manager is guaranteed to exist in the lifetime of the client.
  SyncPointManager* const sync_point_manager_;

  scoped_refptr<SyncPointOrderData> order_data_;

  scoped_refptr<SyncPointClientState> client_state_;

  // Unique namespace/client id pair for this sync point client.
  const CommandBufferNamespace namespace_id_;
  const CommandBufferId command_buffer_id_;

  DISALLOW_COPY_AND_ASSIGN(SyncPointClient);
};

// This class manages the sync points, which allow cross-channel
// synchronization.
class GPU_EXPORT SyncPointManager {
 public:
  SyncPointManager();
  ~SyncPointManager();

  // Returns true if the sync token has been released or if the command buffer
  // does not exist.
  bool IsSyncTokenReleased(const SyncToken& sync_token);

  // If the wait is valid (sync token hasn't been processed or command buffer
  // does not exist), the callback is queued to run when the sync point is
  // released. If the wait is invalid, the callback is NOT run. The callback
  // runs on the thread the sync point is released. Clients should use
  // SyncPointClient::Wait because that uses order data to prevent deadlocks.
  bool Wait(const SyncToken& sync_token,
            uint32_t wait_order_num,
            const base::Closure& callback);

  // Like Wait but runs the callback on the given task runner's thread.
  bool WaitNonThreadSafe(
      const SyncToken& sync_token,
      uint32_t wait_order_num,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::Closure& callback);

  // WaitOutOfOrder allows waiting for a sync token indefinitely, so it
  // should be used with trusted sync tokens only.
  bool WaitOutOfOrder(const SyncToken& trusted_sync_token,
                      const base::Closure& callback);

  // Like WaitOutOfOrder but runs the callback on the given task runner's
  // thread.
  bool WaitOutOfOrderNonThreadSafe(
      const SyncToken& trusted_sync_token,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::Closure& callback);

  // Used by SyncPointClient.
  void RegisterSyncPointClient(scoped_refptr<SyncPointClientState> client_state,
                               CommandBufferNamespace namespace_id,
                               CommandBufferId command_buffer_id);

  void DeregisterSyncPointClient(CommandBufferNamespace namespace_id,
                                 CommandBufferId command_buffer_id);

  // Used by SyncPointOrderData.
  uint32_t GenerateOrderNumber();

 private:
  using ClientStateMap = std::unordered_map<CommandBufferId,
                                            scoped_refptr<SyncPointClientState>,
                                            CommandBufferId::Hasher>;

  scoped_refptr<SyncPointClientState> GetSyncPointClientState(
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id);

  // Order number is global for all clients.
  base::AtomicSequenceNumber global_order_num_;

  // Client map holds a map of clients id to client for each namespace.
  base::Lock client_state_maps_lock_;
  ClientStateMap client_state_maps_[NUM_COMMAND_BUFFER_NAMESPACES];

  DISALLOW_COPY_AND_ASSIGN(SyncPointManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SYNC_POINT_MANAGER_H_

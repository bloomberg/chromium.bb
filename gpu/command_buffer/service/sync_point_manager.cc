// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/sync_point_manager.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"

namespace gpu {

namespace {

void RunOnThread(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 const base::Closure& callback) {
  if (task_runner->BelongsToCurrentThread()) {
    callback.Run();
  } else {
    task_runner->PostTask(FROM_HERE, callback);
  }
}

}  // namespace

scoped_refptr<SyncPointOrderData> SyncPointOrderData::Create() {
  return new SyncPointOrderData;
}

void SyncPointOrderData::Destroy() {
  // Because of circular references between the SyncPointOrderData and
  // SyncPointClientState, we must remove the references on destroy. Releasing
  // the fence syncs in the order fence queue would be redundant at this point
  // because they are assumed to be released on the destruction of the
  // SyncPointClient.
  base::AutoLock auto_lock(lock_);
  destroyed_ = true;
  while (!order_fence_queue_.empty()) {
    order_fence_queue_.pop();
  }
}

uint32_t SyncPointOrderData::GenerateUnprocessedOrderNumber(
    SyncPointManager* sync_point_manager) {
  const uint32_t order_num = sync_point_manager->GenerateOrderNumber();
  base::AutoLock auto_lock(lock_);
  unprocessed_order_num_ = order_num;
  return order_num;
}

void SyncPointOrderData::BeginProcessingOrderNumber(uint32_t order_num) {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  DCHECK_GE(order_num, current_order_num_);
  // Use thread-safe accessors here because |processed_order_num_| and
  // |unprocessed_order_num_| are protected by a lock.
  DCHECK_GT(order_num, processed_order_num());
  DCHECK_LE(order_num, unprocessed_order_num());
  current_order_num_ = order_num;
  paused_ = false;

  // Catch invalid waits which were waiting on fence syncs that do not exist.
  // When we begin processing an order number, we should release any fence
  // syncs which were enqueued but the order number never existed.
  // Release without the lock to avoid possible deadlocks.
  std::vector<OrderFence> ensure_releases;
  {
    base::AutoLock auto_lock(lock_);
    while (!order_fence_queue_.empty()) {
      const OrderFence& order_fence = order_fence_queue_.top();
      if (order_fence_queue_.top().order_num < order_num) {
        ensure_releases.push_back(order_fence);
        order_fence_queue_.pop();
        continue;
      }
      break;
    }
  }

  for (OrderFence& order_fence : ensure_releases) {
    order_fence.client_state->EnsureWaitReleased(order_fence.fence_release,
                                                 order_fence.release_callback);
  }
}

void SyncPointOrderData::PauseProcessingOrderNumber(uint32_t order_num) {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(current_order_num_, order_num);
  DCHECK(!paused_);
  paused_ = true;
}

void SyncPointOrderData::FinishProcessingOrderNumber(uint32_t order_num) {
  DCHECK(processing_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(current_order_num_, order_num);
  DCHECK(!paused_);

  // Catch invalid waits which were waiting on fence syncs that do not exist.
  // When we end processing an order number, we should release any fence syncs
  // which were suppose to be released during this order number.
  // Release without the lock to avoid possible deadlocks.
  std::vector<OrderFence> ensure_releases;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_GT(order_num, processed_order_num_);
    processed_order_num_ = order_num;

    while (!order_fence_queue_.empty()) {
      const OrderFence& order_fence = order_fence_queue_.top();
      if (order_fence_queue_.top().order_num <= order_num) {
        ensure_releases.push_back(order_fence);
        order_fence_queue_.pop();
        continue;
      }
      break;
    }
  }

  for (OrderFence& order_fence : ensure_releases) {
    order_fence.client_state->EnsureWaitReleased(order_fence.fence_release,
                                                 order_fence.release_callback);
  }
}

SyncPointOrderData::OrderFence::OrderFence(
    uint32_t order,
    uint64_t release,
    const base::Closure& callback,
    scoped_refptr<SyncPointClientState> state)
    : order_num(order),
      fence_release(release),
      release_callback(callback),
      client_state(state) {}

SyncPointOrderData::OrderFence::OrderFence(const OrderFence& other) = default;

SyncPointOrderData::OrderFence::~OrderFence() {}

SyncPointOrderData::SyncPointOrderData() {}

SyncPointOrderData::~SyncPointOrderData() {}

bool SyncPointOrderData::ValidateReleaseOrderNumber(
    scoped_refptr<SyncPointClientState> client_state,
    uint32_t wait_order_num,
    uint64_t fence_release,
    const base::Closure& release_callback) {
  base::AutoLock auto_lock(lock_);
  if (destroyed_)
    return false;

  // Release should have a possible unprocessed order number lower than the wait
  // order number.
  if ((processed_order_num_ + 1) >= wait_order_num)
    return false;

  // Release should have more unprocessed numbers if we are waiting.
  if (unprocessed_order_num_ <= processed_order_num_)
    return false;

  // So far it could be valid, but add an order fence guard to be sure it
  // gets released eventually.
  uint32_t expected_order_num =
      std::min(unprocessed_order_num_, wait_order_num);
  order_fence_queue_.push(OrderFence(expected_order_num, fence_release,
                                     release_callback, client_state));
  return true;
}

SyncPointClientState::ReleaseCallback::ReleaseCallback(
    uint64_t release,
    const base::Closure& callback)
    : release_count(release), callback_closure(callback) {}

SyncPointClientState::ReleaseCallback::ReleaseCallback(
    const ReleaseCallback& other) = default;

SyncPointClientState::ReleaseCallback::~ReleaseCallback() {}

SyncPointClientState::SyncPointClientState(
    scoped_refptr<SyncPointOrderData> order_data)
    : order_data_(order_data) {}

SyncPointClientState::~SyncPointClientState() {}

bool SyncPointClientState::IsFenceSyncReleased(uint64_t release) {
  base::AutoLock lock(fence_sync_lock_);
  return release <= fence_sync_release_;
}

bool SyncPointClientState::WaitForRelease(uint64_t release,
                                          uint32_t wait_order_num,
                                          const base::Closure& callback) {
  // Lock must be held the whole time while we validate otherwise it could be
  // released while we are checking.
  {
    base::AutoLock auto_lock(fence_sync_lock_);
    if (release > fence_sync_release_ &&
        order_data_->ValidateReleaseOrderNumber(this, wait_order_num, release,
                                                callback)) {
      // Add the callback which will be called upon release.
      release_callback_queue_.push(ReleaseCallback(release, callback));
      return true;
    }
  }
  // Already released, do not run the callback.
  return false;
}

void SyncPointClientState::ReleaseFenceSync(uint64_t release) {
  // Call callbacks without the lock to avoid possible deadlocks.
  std::vector<base::Closure> callback_list;
  {
    base::AutoLock auto_lock(fence_sync_lock_);

    DLOG_IF(ERROR, release <= fence_sync_release_)
        << "Client submitted fence releases out of order.";
    fence_sync_release_ = release;

    while (!release_callback_queue_.empty() &&
           release_callback_queue_.top().release_count <= release) {
      callback_list.push_back(release_callback_queue_.top().callback_closure);
      release_callback_queue_.pop();
    }
  }

  for (const base::Closure& closure : callback_list) {
    closure.Run();
  }
}

void SyncPointClientState::EnsureWaitReleased(uint64_t release,
                                              const base::Closure& callback) {
  // Call callbacks without the lock to avoid possible deadlocks.
  bool call_callback = false;
  {
    base::AutoLock auto_lock(fence_sync_lock_);
    if (release <= fence_sync_release_)
      return;

    std::vector<ReleaseCallback> popped_callbacks;
    popped_callbacks.reserve(release_callback_queue_.size());

    while (!release_callback_queue_.empty() &&
           release_callback_queue_.top().release_count <= release) {
      const ReleaseCallback& top_item = release_callback_queue_.top();
      if (top_item.release_count == release &&
          top_item.callback_closure.Equals(callback)) {
        // Call the callback, and discard this item from the callback queue.
        call_callback = true;
      } else {
        // Store the item to be placed back into the callback queue later.
        popped_callbacks.push_back(top_item);
      }
      release_callback_queue_.pop();
    }

    // Add back in popped items.
    for (const ReleaseCallback& popped_callback : popped_callbacks) {
      release_callback_queue_.push(popped_callback);
    }
  }

  if (call_callback) {
    // This effectively releases the wait without releasing the fence.
    callback.Run();
  }
}

SyncPointClient::SyncPointClient(SyncPointManager* sync_point_manager,
                                 scoped_refptr<SyncPointOrderData> order_data,
                                 CommandBufferNamespace namespace_id,
                                 CommandBufferId command_buffer_id)
    : sync_point_manager_(sync_point_manager),
      order_data_(order_data),
      client_state_(new SyncPointClientState(order_data)),
      namespace_id_(namespace_id),
      command_buffer_id_(command_buffer_id) {
  sync_point_manager_->RegisterSyncPointClient(client_state_, namespace_id,
                                               command_buffer_id);
}

SyncPointClient::~SyncPointClient() {
  // Release all fences on destruction.
  client_state_->ReleaseFenceSync(UINT64_MAX);
  sync_point_manager_->DeregisterSyncPointClient(namespace_id_,
                                                 command_buffer_id_);
}

bool SyncPointClient::Wait(const SyncToken& sync_token,
                           const base::Closure& callback) {
  // Validate that this Wait call is between BeginProcessingOrderNumber() and
  // FinishProcessingOrderNumber(), or else we may deadlock.
  DCHECK(order_data_->IsProcessingOrderNumber());
  if (sync_token.namespace_id() == namespace_id_ &&
      sync_token.command_buffer_id() == command_buffer_id_) {
    return false;
  }
  uint32_t wait_order_number = order_data_->current_order_num();
  return sync_point_manager_->Wait(sync_token, wait_order_number, callback);
}

bool SyncPointClient::WaitNonThreadSafe(
    const SyncToken& sync_token,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& callback) {
  return Wait(sync_token, base::Bind(&RunOnThread, task_runner, callback));
}

void SyncPointClient::ReleaseFenceSync(uint64_t release) {
  // Validate that this Release call is between BeginProcessingOrderNumber() and
  // FinishProcessingOrderNumber(), or else we may deadlock.
  DCHECK(order_data_->IsProcessingOrderNumber());
  client_state_->ReleaseFenceSync(release);
}

SyncPointManager::SyncPointManager() {
  global_order_num_.GetNext();
}

SyncPointManager::~SyncPointManager() {
  for (const ClientStateMap& client_state_map : client_state_maps_)
    DCHECK(client_state_map.empty());
}

bool SyncPointManager::IsSyncTokenReleased(const SyncToken& sync_token) {
  scoped_refptr<SyncPointClientState> release_state = GetSyncPointClientState(
      sync_token.namespace_id(), sync_token.command_buffer_id());
  if (release_state)
    return release_state->IsFenceSyncReleased(sync_token.release_count());
  return true;
}

bool SyncPointManager::Wait(const SyncToken& sync_token,
                            uint32_t wait_order_num,
                            const base::Closure& callback) {
  scoped_refptr<SyncPointClientState> release_state = GetSyncPointClientState(
      sync_token.namespace_id(), sync_token.command_buffer_id());
  if (release_state &&
      release_state->WaitForRelease(sync_token.release_count(), wait_order_num,
                                    callback)) {
    return true;
  }
  // Do not run callback if wait is invalid.
  return false;
}

bool SyncPointManager::WaitNonThreadSafe(
    const SyncToken& sync_token,
    uint32_t wait_order_num,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& callback) {
  return Wait(sync_token, wait_order_num,
              base::Bind(&RunOnThread, task_runner, callback));
}

bool SyncPointManager::WaitOutOfOrder(const SyncToken& trusted_sync_token,
                                      const base::Closure& callback) {
  // No order number associated with the current execution context, using
  // UINT32_MAX will just assume the release is in the SyncPointClientState's
  // order numbers to be executed.
  return Wait(trusted_sync_token, UINT32_MAX, callback);
}

bool SyncPointManager::WaitOutOfOrderNonThreadSafe(
    const SyncToken& trusted_sync_token,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& callback) {
  return WaitOutOfOrder(trusted_sync_token,
                        base::Bind(&RunOnThread, task_runner, callback));
}

void SyncPointManager::RegisterSyncPointClient(
    scoped_refptr<SyncPointClientState> client_state,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  DCHECK_GE(namespace_id, 0);
  DCHECK_LT(static_cast<size_t>(namespace_id), arraysize(client_state_maps_));

  base::AutoLock auto_lock(client_state_maps_lock_);
  DCHECK(!client_state_maps_[namespace_id].count(command_buffer_id));
  client_state_maps_[namespace_id].insert(
      std::make_pair(command_buffer_id, client_state));
}

void SyncPointManager::DeregisterSyncPointClient(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  DCHECK_GE(namespace_id, 0);
  DCHECK_LT(static_cast<size_t>(namespace_id), arraysize(client_state_maps_));

  base::AutoLock auto_lock(client_state_maps_lock_);
  DCHECK(client_state_maps_[namespace_id].count(command_buffer_id));
  client_state_maps_[namespace_id].erase(command_buffer_id);
}

uint32_t SyncPointManager::GenerateOrderNumber() {
  return global_order_num_.GetNext();
}

scoped_refptr<SyncPointClientState> SyncPointManager::GetSyncPointClientState(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  if (namespace_id >= 0) {
    DCHECK_LT(static_cast<size_t>(namespace_id), arraysize(client_state_maps_));
    base::AutoLock auto_lock(client_state_maps_lock_);
    ClientStateMap& client_state_map = client_state_maps_[namespace_id];
    auto it = client_state_map.find(command_buffer_id);
    if (it != client_state_map.end())
      return it->second;
  }
  return nullptr;
}

}  // namespace gpu

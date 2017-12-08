// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_
#define GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "gpu/command_buffer/common/transfer_cache_entry_id.h"
#include "ui/gfx/overlay_transform.h"

namespace cc {
class ClientTransferCacheEntry;
}

namespace gfx {
class Rect;
class RectF;
}

namespace gpu {

struct SyncToken;

class ContextSupport {
 public:
  // Flush any outstanding ordering barriers for all contexts.
  virtual void FlushPendingWork() = 0;

  // Runs |callback| when the given sync token is signalled. The sync token may
  // belong to any context.
  virtual void SignalSyncToken(const SyncToken& sync_token,
                               base::OnceClosure callback) = 0;

  // Returns true if the given sync token has been signaled. The sync token must
  // belong to this context. This may be called from any thread.
  virtual bool IsSyncTokenSignaled(const SyncToken& sync_token) = 0;

  // Runs |callback| when a query created via glCreateQueryEXT() has cleared
  // passed the glEndQueryEXT() point.
  virtual void SignalQuery(uint32_t query, base::OnceClosure callback) = 0;

  // Indicates whether the context should aggressively free allocated resources.
  // If set to true, the context will purge all temporary resources when
  // flushed.
  virtual void SetAggressivelyFreeResources(
      bool aggressively_free_resources) = 0;

  virtual void Swap() = 0;
  virtual void SwapWithBounds(const std::vector<gfx::Rect>& rects) = 0;
  virtual void PartialSwapBuffers(const gfx::Rect& sub_buffer) = 0;
  virtual void CommitOverlayPlanes() = 0;

  // Schedule a texture to be presented as an overlay synchronously with the
  // primary surface during the next buffer swap or CommitOverlayPlanes.
  // This method is not stateful and needs to be re-scheduled every frame.
  virtual void ScheduleOverlayPlane(int plane_z_order,
                                    gfx::OverlayTransform plane_transform,
                                    unsigned overlay_texture_id,
                                    const gfx::Rect& display_bounds,
                                    const gfx::RectF& uv_rect) = 0;

  // Returns an ID that can be used to globally identify the share group that
  // this context's resources belong to.
  virtual uint64_t ShareGroupTracingGUID() const = 0;

  // Sets a callback to be run when an error occurs.
  virtual void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) = 0;

  // Indicates whether a snapshot is associated with the next swap.
  virtual void SetSnapshotRequested() = 0;

  // Allows locking a GPU discardable texture from any thread. Any successful
  // call to ThreadSafeShallowLockDiscardableTexture must be paired with a
  // later call to CompleteLockDiscardableTexureOnContextThread.
  virtual bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) = 0;

  // Must be called on the context's thread, only following a successful call
  // to ThreadSafeShallowLockDiscardableTexture.
  virtual void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) = 0;

  // Access to transfer cache functionality for OOP raster. Only
  // ThreadsafeLockTransferCacheEntry can be accessed without holding the
  // context lock.
  virtual gpu::TransferCacheEntryId CreateTransferCacheEntry(
      const cc::ClientTransferCacheEntry& entry) = 0;
  virtual bool ThreadsafeLockTransferCacheEntry(
      gpu::TransferCacheEntryId id) = 0;
  virtual void UnlockTransferCacheEntry(gpu::TransferCacheEntryId id) = 0;
  virtual void DeleteTransferCacheEntry(gpu::TransferCacheEntryId id) = 0;

 protected:
  ContextSupport() = default;
  virtual ~ContextSupport() = default;
};

}

#endif  // GPU_COMMAND_BUFFER_CLIENT_CONTEXT_SUPPORT_H_

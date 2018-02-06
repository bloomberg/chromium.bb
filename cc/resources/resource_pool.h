// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_POOL_H_
#define CC_RESOURCES_RESOURCE_POOL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/memory_coordinator_client.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/unguessable_token.h"
#include "cc/cc_export.h"
#include "cc/resources/resource.h"
#include "cc/resources/scoped_resource.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {

class CC_EXPORT ResourcePool : public base::trace_event::MemoryDumpProvider,
                               public base::MemoryCoordinatorClient {
  class PoolResource;

 public:
  // Delay before a resource is considered expired.
  static constexpr base::TimeDelta kDefaultExpirationDelay =
      base::TimeDelta::FromSeconds(5);

  // A base class to hold ownership of gpu backed PoolResources. Allows the
  // client to define destruction semantics.
  class GpuBacking {
   public:
    virtual ~GpuBacking() = default;

    gpu::Mailbox mailbox;
    gpu::SyncToken mailbox_sync_token;
    uint32_t texture_target;
    gpu::SyncToken returned_sync_token;

    // Guids for for memory dumps. This guid will always be valid.
    virtual base::trace_event::MemoryAllocatorDumpGuid MemoryDumpGuid(
        uint64_t tracing_process_id) = 0;
    // Some gpu resources can be shared memory-backed, and this guid should be
    // prefered in that case. But if not then this will be empty.
    virtual base::UnguessableToken SharedMemoryGuid() = 0;
  };

  // Scoped move-only object returned when getting a resource from the pool.
  // Ownership must be given back to the pool to release the resource.
  class InUsePoolResource {
   public:
    InUsePoolResource() = default;
    ~InUsePoolResource() {
      DCHECK(!resource_) << "Must be returned to ResourcePool to be freed.";
    }

    InUsePoolResource(InUsePoolResource&& other) {
      is_gpu_ = other.is_gpu_;
      resource_ = other.resource_;
      other.resource_ = nullptr;
    }
    InUsePoolResource& operator=(InUsePoolResource&& other) {
      is_gpu_ = other.is_gpu_;
      resource_ = other.resource_;
      other.resource_ = nullptr;
      return *this;
    }

    InUsePoolResource(const InUsePoolResource&) = delete;
    InUsePoolResource& operator=(const InUsePoolResource&) = delete;

    explicit operator bool() const { return !!resource_; }

    const gfx::Size& size() const { return resource_->size(); }
    const viz::ResourceFormat& format() const { return resource_->format(); }
    const gfx::ColorSpace& color_space() const {
      return resource_->color_space();
    }
    // The ResourceId when the backing is given to the ResourceProvider for
    // export to the display compositor.
    const viz::ResourceId& resource_id_for_export() const {
      // The ResourceId should not be accessed before it is created!
      DCHECK(resource_->resource_id());
      return resource_->resource_id();
    }

    // Only valid when the ResourcePool is vending gpu-backed resources.
    const viz::ResourceId& gpu_backing_resource_id() const {
      DCHECK(is_gpu_);
      return resource_->resource_id();
    }

    // Only valid when the ResourcePool is vending texture-backed resources.
    GpuBacking* gpu_backing() const {
      DCHECK(is_gpu_);
      return resource_->gpu_backing();
    }
    void set_gpu_backing(std::unique_ptr<GpuBacking> gpu) const {
      DCHECK(is_gpu_);
      return resource_->set_gpu_backing(std::move(gpu));
    }

    // Only valid when the ResourcePool is vending software-backed resources.
    viz::SharedBitmap* shared_bitmap() const {
      DCHECK(!is_gpu_);
      return resource_->shared_bitmap();
    }
    void set_shared_bitmap(
        std::unique_ptr<viz::SharedBitmap> shared_bitmap) const {
      DCHECK(!is_gpu_);
      DCHECK(!resource_->shared_bitmap());
      resource_->set_shared_bitmap(std::move(shared_bitmap));
    }

   private:
    friend ResourcePool;
    explicit InUsePoolResource(PoolResource* resource, bool is_gpu)
        : is_gpu_(is_gpu), resource_(resource) {}
    void SetWasFreedByResourcePool() { resource_ = nullptr; }

    bool is_gpu_ = false;
    PoolResource* resource_ = nullptr;
  };

  // Constructor for creating Gpu memory buffer resources.
  ResourcePool(LayerTreeResourceProvider* resource_provider,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner,
               gfx::BufferUsage usage,
               const base::TimeDelta& expiration_delay,
               bool disallow_non_exact_reuse);

  // Constructor for creating standard Gpu resources.
  ResourcePool(LayerTreeResourceProvider* resource_provider,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner,
               viz::ResourceTextureHint hint,
               const base::TimeDelta& expiration_delay,
               bool disallow_non_exact_reuse);

  // Constructor for creating software resources.
  ResourcePool(LayerTreeResourceProvider* resource_provider,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner,
               const base::TimeDelta& expiration_delay,
               bool disallow_non_exact_reuse);

  ~ResourcePool() override;

  // Tries to reuse a resource. If none are available, makes a new one.
  InUsePoolResource AcquireResource(const gfx::Size& size,
                                    viz::ResourceFormat format,
                                    const gfx::ColorSpace& color_space);

  // Tries to acquire the resource with |previous_content_id| for us in partial
  // raster. If successful, this function will retun the invalidated rect which
  // must be re-rastered in |total_invalidated_rect|.
  InUsePoolResource TryAcquireResourceForPartialRaster(
      uint64_t new_content_id,
      const gfx::Rect& new_invalidated_rect,
      uint64_t previous_content_id,
      gfx::Rect* total_invalidated_rect);

  // Gives the InUsePoolResource a |resource_id_for_export()| in order to allow
  // exporting of the resource to the display compositor. This must be called
  // with a resource only after it has a backing allocated for it. Initially an
  // acquired InUsePoolResource will be only metadata, and the backing is given
  // to it by code which is aware of the expected backing type - currently by
  // RasterBufferProvider::AcquireBufferForRaster().
  void PrepareForExport(const InUsePoolResource& resource);

  // Called when a resource's content has been fully replaced (and is completely
  // valid). Updates the resource's content ID to its new value.
  void OnContentReplaced(const ResourcePool::InUsePoolResource& in_use_resource,
                         uint64_t content_id);
  void ReleaseResource(InUsePoolResource resource);

  void SetResourceUsageLimits(size_t max_memory_usage_bytes,
                              size_t max_resource_count);
  void ReduceResourceUsage();
  bool ResourceUsageTooHigh();

  // Must be called regularly to move resources from the busy pool to the unused
  // pool.
  void CheckBusyResources();

  size_t memory_usage_bytes() const { return in_use_memory_usage_bytes_; }
  size_t resource_count() const { return in_use_resources_.size(); }

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Overriden from base::MemoryCoordinatorClient.
  void OnPurgeMemory() override;
  void OnMemoryStateChange(base::MemoryState state) override;

  size_t GetTotalMemoryUsageForTesting() const {
    return total_memory_usage_bytes_;
  }
  size_t GetTotalResourceCountForTesting() const {
    return total_resource_count_;
  }
  size_t GetBusyResourceCountForTesting() const {
    return busy_resources_.size();
  }
  bool AllowsNonExactReUseForTesting() const {
    return !disallow_non_exact_reuse_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourcePoolTest, ReuseResource);
  FRIEND_TEST_ALL_PREFIXES(ResourcePoolTest, ExactRequestsRespected);
  class PoolResource {
   public:
    PoolResource(size_t unique_id,
                 const gfx::Size& size,
                 viz::ResourceFormat format,
                 const gfx::ColorSpace& color_space,
                 viz::ResourceId resource_id);
    ~PoolResource();

    size_t unique_id() const { return unique_id_; }
    const gfx::Size& size() const { return size_; }
    const viz::ResourceFormat& format() const { return format_; }
    const gfx::ColorSpace& color_space() const { return color_space_; }

    const viz::ResourceId& resource_id() const { return resource_id_; }
    void set_resource_id(viz::ResourceId id) { resource_id_ = id; }

    GpuBacking* gpu_backing() const { return gpu_backing_.get(); }
    void set_gpu_backing(std::unique_ptr<GpuBacking> gpu) {
      gpu_backing_ = std::move(gpu);
    }

    viz::SharedBitmap* shared_bitmap() const { return shared_bitmap_.get(); }
    void set_shared_bitmap(std::unique_ptr<viz::SharedBitmap> shared_bitmap) {
      shared_bitmap_ = std::move(shared_bitmap);
    }

    uint64_t content_id() const { return content_id_; }
    void set_content_id(uint64_t content_id) { content_id_ = content_id; }

    base::TimeTicks last_usage() const { return last_usage_; }
    void set_last_usage(base::TimeTicks time) { last_usage_ = time; }

    const gfx::Rect& invalidated_rect() const { return invalidated_rect_; }
    void set_invalidated_rect(const gfx::Rect& invalidated_rect) {
      invalidated_rect_ = invalidated_rect;
    }

    void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                      const LayerTreeResourceProvider* resource_provider,
                      bool dump_parent,
                      bool is_free) const;

   private:
    const size_t unique_id_;
    const gfx::Size size_;
    const viz::ResourceFormat format_;
    const gfx::ColorSpace color_space_;

    uint64_t content_id_ = 0;
    base::TimeTicks last_usage_;
    gfx::Rect invalidated_rect_;

    // An id used to name the backing for transfer to the display compositor.
    viz::ResourceId resource_id_ = 0;

    // The backing for gpu resources. Initially null for resources given
    // out by ResourcePool, to be filled in by the client. Is destroyed on the
    // compositor thread.
    std::unique_ptr<GpuBacking> gpu_backing_;

    // The backing for software resources. Initially null for resources given
    // out by ResourcePool, to be filled in by the client. Is destroyed on the
    // compositor thread.
    std::unique_ptr<viz::SharedBitmap> shared_bitmap_;
  };

  // Callback from the ResourceProvider to notify when an exported PoolResource
  // is not busy and may be reused.
  void OnResourceReleased(size_t unique_id,
                          const gpu::SyncToken& sync_token,
                          bool lost);

  // Tries to reuse a resource. Returns |nullptr| if none are available.
  PoolResource* ReuseResource(const gfx::Size& size,
                              viz::ResourceFormat format,
                              const gfx::ColorSpace& color_space);

  // Creates a new resource without trying to reuse an old one.
  PoolResource* CreateResource(const gfx::Size& size,
                               viz::ResourceFormat format,
                               const gfx::ColorSpace& color_space);

  void DidFinishUsingResource(std::unique_ptr<PoolResource> resource);
  void DeleteResource(std::unique_ptr<PoolResource> resource);
  static void UpdateResourceContentIdAndInvalidation(
      PoolResource* resource,
      uint64_t new_content_id,
      const gfx::Rect& new_invalidated_rect);

  // Functions which manage periodic eviction of expired resources.
  void ScheduleEvictExpiredResourcesIn(base::TimeDelta time_from_now);
  void EvictExpiredResources();
  void EvictResourcesNotUsedSince(base::TimeTicks time_limit);
  bool HasEvictableResources() const;
  base::TimeTicks GetUsageTimeForLRUResource() const;

  LayerTreeResourceProvider* const resource_provider_ = nullptr;
  const bool use_gpu_resources_ = false;
  const bool use_gpu_memory_buffers_ = false;
  const gfx::BufferUsage usage_ = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
  const viz::ResourceTextureHint hint_ = viz::ResourceTextureHint::kDefault;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const base::TimeDelta resource_expiration_delay_;
  const bool disallow_non_exact_reuse_ = false;

  size_t next_resource_unique_id_ = 1;
  size_t max_memory_usage_bytes_ = 0;
  size_t max_resource_count_ = 0;
  size_t in_use_memory_usage_bytes_ = 0;
  size_t total_memory_usage_bytes_ = 0;
  size_t total_resource_count_ = 0;
  bool evict_expired_resources_pending_ = false;
  bool evict_busy_resources_when_unused_ = false;

  // Holds most recently used resources at the front of the queue.
  base::circular_deque<std::unique_ptr<PoolResource>> unused_resources_;
  base::circular_deque<std::unique_ptr<PoolResource>> busy_resources_;

  // Map from the PoolResource |unique_id| to the PoolResource.
  std::map<size_t, std::unique_ptr<PoolResource>> in_use_resources_;

  base::WeakPtrFactory<ResourcePool> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePool);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_POOL_H_

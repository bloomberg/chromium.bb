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
#include "base/trace_event/memory_dump_provider.h"
#include "cc/cc_export.h"
#include "cc/resources/resource.h"
#include "cc/resources/scoped_resource.h"
#include "components/viz/common/resources/resource_format.h"
#include "ui/gfx/geometry/rect.h"

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
    // The ResourceId when the backing is given to the ResourceProvider for
    // export to the display compositor.
    const viz::ResourceId& resource_id_for_export() const {
      return resource_->resource_id();
    }

    // Only valid when the ResourcePool is vending software-backed resources.
    const viz::ResourceId& software_backing_resource_id() const {
      DCHECK(!is_gpu_);
      return resource_->resource_id();
    }

    // Only valid when the ResourcePool is vending gpu-backed resources.
    const viz::ResourceId& gpu_backing_resource_id() const {
      DCHECK(is_gpu_);
      return resource_->resource_id();
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
                      bool is_free) const;

   private:
    const size_t unique_id_;
    const gfx::Size size_;
    const viz::ResourceFormat format_;
    const gfx::ColorSpace color_space_;

    uint64_t content_id_ = 0;
    base::TimeTicks last_usage_;
    gfx::Rect invalidated_rect_;

    viz::ResourceId resource_id_;
    // TODO(crbug.com/730660): Own a backing for software resources here,
    // instead of owning it through the |resource_id_|.
  };

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

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_RESOURCE_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/cc_export.h"
#include "cc/resources/return_callback.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource.h"
#include "components/viz/common/resources/resource_fence.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/resource_settings.h"
#include "components/viz/common/resources/resource_texture_hint.h"
#include "components/viz/common/resources/resource_type.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
class GpuMemoryBufferManager;
namespace gles {
class GLES2Interface;
}
}

namespace viz {
class SharedBitmapManager;
}  // namespace viz

namespace cc {
class TextureIdAllocator;

// This class provides abstractions for allocating and transferring resources
// between modules/threads/processes. It abstracts away GL textures vs
// GpuMemoryBuffers vs software bitmaps behind a single ResourceId so that
// code in common can hold onto ResourceIds, as long as the code using them
// knows the correct type.
//
// The resource's underlying type is accessed through Read and Write locks that
// help to safeguard correct usage with DCHECKs. All resources held in
// ResourceProvider are immutable - they can not change format or size once
// they are created, only their contents.
//
// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT ResourceProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  using ResourceIdArray = std::vector<viz::ResourceId>;
  using ResourceIdMap = std::unordered_map<viz::ResourceId, viz::ResourceId>;

  ResourceProvider(viz::ContextProvider* compositor_context_provider,
                   viz::SharedBitmapManager* shared_bitmap_manager,
                   gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                   bool delegated_sync_points_required,
                   const viz::ResourceSettings& resource_settings,
                   viz::ResourceId next_id);
  ~ResourceProvider() override;

  void Initialize();

  bool IsSoftware() const { return !compositor_context_provider_; }

  void DidLoseContextProvider() { lost_context_provider_ = true; }

  int max_texture_size() const { return settings_.max_texture_size; }
  viz::ResourceFormat best_texture_format() const {
    return settings_.best_texture_format;
  }
  viz::ResourceFormat best_render_buffer_format() const {
    return settings_.best_render_buffer_format;
  }
  viz::ResourceFormat YuvResourceFormat(int bits) const;
  bool use_sync_query() const { return settings_.use_sync_query; }
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() {
    return gpu_memory_buffer_manager_;
  }
  size_t num_resources() const { return resources_.size(); }

  bool IsTextureFormatSupported(viz::ResourceFormat format) const;

  // Returns true if the provided |format| can be used as a render buffer.
  // Note that render buffer support implies texture support.
  bool IsRenderBufferFormatSupported(viz::ResourceFormat format) const;

  bool IsGpuMemoryBufferFormatSupported(viz::ResourceFormat format,
                                        gfx::BufferUsage usage) const;

  // Checks whether a resource is in use by a consumer.
  bool InUseByConsumer(viz::ResourceId id);

  bool IsLost(viz::ResourceId id);

  void LoseResourceForTesting(viz::ResourceId id);
  void EnableReadLockFencesForTesting(viz::ResourceId id);

  // Producer interface.
  viz::ResourceType GetResourceType(viz::ResourceId id);
  GLenum GetResourceTextureTarget(viz::ResourceId id);
  viz::ResourceTextureHint GetTextureHint(viz::ResourceId id);

  // Creates a resource of the default resource type.
  viz::ResourceId CreateResource(const gfx::Size& size,
                                 viz::ResourceTextureHint hint,
                                 viz::ResourceFormat format,
                                 const gfx::ColorSpace& color_space);

  // Creates a resource for a particular texture target (the distinction between
  // texture targets has no effect in software mode).
  viz::ResourceId CreateGpuMemoryBufferResource(
      const gfx::Size& size,
      viz::ResourceTextureHint hint,
      viz::ResourceFormat format,
      gfx::BufferUsage usage,
      const gfx::ColorSpace& color_space);

  void DeleteResource(viz::ResourceId id);
  // In the case of GPU resources, we may need to flush the GL context to ensure
  // that texture deletions are seen in a timely fashion. This function should
  // be called after texture deletions that may happen during an idle state.
  void FlushPendingDeletions() const;

  // Update pixels from image, copying source_rect (in image) to dest_offset (in
  // the resource).
  void CopyToResource(viz::ResourceId id,
                      const uint8_t* image,
                      const gfx::Size& image_size);


  // The following lock classes are part of the ResourceProvider API and are
  // needed to read and write the resource contents. The user must ensure
  // that they only use GL locks on GL resources, etc, and this is enforced
  // by assertions.
  class CC_EXPORT ScopedWriteLockGL {
   public:
    ScopedWriteLockGL(ResourceProvider* resource_provider,
                      viz::ResourceId resource_id);
    ~ScopedWriteLockGL();

    GLenum target() const { return target_; }
    viz::ResourceFormat format() const { return format_; }
    const gfx::Size& size() const { return size_; }
    const gfx::ColorSpace& color_space_for_raster() const {
      return color_space_;
    }

    GrPixelConfig PixelConfig() const;

    void set_allocated() { allocated_ = true; }

    void set_sync_token(const gpu::SyncToken& sync_token) {
      sync_token_ = sync_token;
      has_sync_token_ = true;
    }

    void set_synchronized() { synchronized_ = true; }

    void set_generate_mipmap() { generate_mipmap_ = true; }

    // Returns texture id on compositor context, allocating if necessary.
    GLuint GetTexture();

    // Creates mailbox that can be consumed on another context.
    void CreateMailbox();

    // Creates a texture id, allocating if necessary, on the given context. The
    // texture id must be deleted by the caller.
    GLuint ConsumeTexture(gpu::gles2::GLES2Interface* gl);

   private:
    void LazyAllocate(gpu::gles2::GLES2Interface* gl, GLuint texture_id);

    ResourceProvider* const resource_provider_;
    const viz::ResourceId resource_id_;

    // The following are copied from the resource.
    gfx::Size size_;
    viz::ResourceFormat format_;
    gfx::ColorSpace color_space_;
    GLuint texture_id_;
    GLenum target_;
    viz::ResourceTextureHint hint_;
    gpu::Mailbox mailbox_;
    bool is_overlay_;
    bool allocated_;

    // Set by the user.
    gpu::SyncToken sync_token_;
    bool has_sync_token_ = false;
    bool synchronized_ = false;
    bool generate_mipmap_ = false;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGL);
  };

  // TODO(sunnyps): Move to //components/viz/common/gl_helper.h ?
  class CC_EXPORT ScopedSkSurface {
   public:
    ScopedSkSurface(GrContext* gr_context,
                    GLuint texture_id,
                    GLenum texture_target,
                    const gfx::Size& size,
                    viz::ResourceFormat format,
                    bool use_distance_field_text,
                    bool can_use_lcd_text,
                    int msaa_sample_count);
    ~ScopedSkSurface();

    SkSurface* surface() const { return surface_.get(); }

   private:
    sk_sp<SkSurface> surface_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSkSurface);
  };

  class CC_EXPORT ScopedWriteLockSoftware {
   public:
    ScopedWriteLockSoftware(ResourceProvider* resource_provider,
                            viz::ResourceId resource_id);
    ~ScopedWriteLockSoftware();

    SkBitmap& sk_bitmap() { return sk_bitmap_; }
    bool valid() const { return !!sk_bitmap_.getPixels(); }
    const gfx::ColorSpace& color_space_for_raster() const {
      return color_space_;
    }

   private:
    ResourceProvider* const resource_provider_;
    const viz::ResourceId resource_id_;
    gfx::ColorSpace color_space_;
    SkBitmap sk_bitmap_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockSoftware);
  };

  class CC_EXPORT SynchronousFence : public viz::ResourceFence {
   public:
    explicit SynchronousFence(gpu::gles2::GLES2Interface* gl);

    // viz::ResourceFence implementation.
    void Set() override;
    bool HasPassed() override;
    void Wait() override;

    // Returns true if fence has been set but not yet synchornized.
    bool has_synchronized() const { return has_synchronized_; }

   private:
    ~SynchronousFence() override;

    void Synchronize();

    gpu::gles2::GLES2Interface* gl_;
    bool has_synchronized_;

    DISALLOW_COPY_AND_ASSIGN(SynchronousFence);
  };

  // For tests only! This prevents detecting uninitialized reads.
  // Use SetPixels or LockForWrite to allocate implicitly.
  void AllocateForTesting(viz::ResourceId id);

  // For tests only!
  void CreateForTesting(viz::ResourceId id);

  // Indicates if we can currently lock this resource for write.
  bool CanLockForWrite(viz::ResourceId id);

  // Indicates if this resource may be used for a hardware overlay plane.
  bool IsOverlayCandidate(viz::ResourceId id);

  // Return the format of the underlying buffer that can be used for scanout.
  gfx::BufferFormat GetBufferFormat(viz::ResourceId id);

#if defined(OS_ANDROID)
  // Indicates if this resource is backed by an Android SurfaceTexture, and thus
  // can't really be promoted to an overlay.
  bool IsBackedBySurfaceTexture(viz::ResourceId id);

  // Indicates if this resource wants to receive promotion hints.
  bool WantsPromotionHintForTesting(viz::ResourceId id);

  // Return the number of resources that request promotion hints.
  size_t CountPromotionHintRequestsForTesting();
#endif

  static GLint GetActiveTextureUnit(gpu::gles2::GLES2Interface* gl);

  static gpu::SyncToken GenerateSyncTokenHelper(gpu::gles2::GLES2Interface* gl);

  GLenum GetImageTextureTarget(gfx::BufferUsage usage,
                               viz::ResourceFormat format) const;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  int tracing_id() const { return tracing_id_; }

 protected:
  using ResourceMap =
      std::unordered_map<viz::ResourceId, viz::internal::Resource>;

  viz::internal::Resource* InsertResource(viz::ResourceId id,
                                          viz::internal::Resource resource);

  viz::internal::Resource* GetResource(viz::ResourceId id);

  viz::internal::Resource* LockForWrite(viz::ResourceId id);
  void UnlockForWrite(viz::internal::Resource* resource);

  void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                    const viz::internal::Resource* resource);

  void CreateAndBindImage(viz::internal::Resource* resource);

  // Binds the given GL resource to a texture target for sampling using the
  // specified filter for both minification and magnification. Returns the
  // texture target used. The resource must be locked for reading.
  GLenum BindForSampling(viz::ResourceId resource_id,
                         GLenum unit,
                         GLenum filter);

  gfx::ColorSpace GetResourceColorSpaceForRaster(
      const viz::internal::Resource* resource) const;

  enum DeleteStyle {
    NORMAL,
    FOR_SHUTDOWN,
  };

  void DeleteResourceInternal(ResourceMap::iterator it, DeleteStyle style);

  void CreateMailbox(viz::internal::Resource* resource);
  void WaitSyncTokenInternal(viz::internal::Resource* resource);

  bool ReadLockFenceHasPassed(const viz::internal::Resource* resource) {
    return !resource->read_lock_fence.get() ||
           resource->read_lock_fence->HasPassed();
  }

  // Returns null if we do not have a viz::ContextProvider.
  gpu::gles2::GLES2Interface* ContextGL() const;

  // Holds const settings for the ResourceProvider. Never changed after init.
  struct Settings {
    Settings(viz::ContextProvider* compositor_context_provider,
             bool delegated_sync_points_needed,
             const viz::ResourceSettings& resource_settings);

    int max_texture_size = 0;
    bool use_texture_storage = false;
    bool use_texture_format_bgra = false;
    bool use_texture_usage_hint = false;
    bool use_texture_npot = false;
    bool use_sync_query = false;
    bool use_texture_storage_image = false;
    viz::ResourceType default_resource_type = viz::ResourceType::kTexture;
    viz::ResourceFormat yuv_resource_format = viz::LUMINANCE_8;
    viz::ResourceFormat yuv_highbit_resource_format = viz::LUMINANCE_8;
    viz::ResourceFormat best_texture_format = viz::RGBA_8888;
    viz::ResourceFormat best_render_buffer_format = viz::RGBA_8888;
    bool use_gpu_memory_buffer_resources = false;
    bool delegated_sync_points_required = false;
  } const settings_;

  ResourceMap resources_;

  // Keep track of whether deleted resources should be batched up or returned
  // immediately.
  bool batch_return_resources_ = false;
  // Maps from a child id to the set of resources to be returned to it.
  base::small_map<std::map<int, ResourceIdArray>> batched_returning_resources_;

  viz::ContextProvider* compositor_context_provider_;
  viz::SharedBitmapManager* shared_bitmap_manager_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  viz::ResourceId next_id_;
  int next_child_;

  bool lost_context_provider_;

  THREAD_CHECKER(thread_checker_);

#if defined(OS_ANDROID)
  // Set of resource Ids that would like to be notified about promotion hints.
  viz::ResourceIdSet wants_promotion_hints_set_;
#endif

 private:
  viz::ResourceId CreateGpuTextureResource(const gfx::Size& size,
                                           viz::ResourceTextureHint hint,
                                           viz::ResourceFormat format,
                                           const gfx::ColorSpace& color_space);

  viz::ResourceId CreateBitmapResource(const gfx::Size& size,
                                       const gfx::ColorSpace& color_space);

  void CreateTexture(viz::internal::Resource* resource);

  bool IsGLContextLost() const;

  std::unique_ptr<TextureIdAllocator> texture_id_allocator_;
  viz::BufferUsageAndFormatList texture_target_exception_list_;

  // A process-unique ID used for disambiguating memory dumps from different
  // resource providers.
  int tracing_id_;

  DISALLOW_COPY_AND_ASSIGN(ResourceProvider);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_PROVIDER_H_

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_RESOURCE_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
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
#include "cc/base/resource_id.h"
#include "cc/cc_export.h"
#include "cc/output/output_surface.h"
#include "cc/output/renderer_settings.h"
#include "cc/resources/release_callback_impl.h"
#include "cc/resources/return_callback.h"
#include "cc/resources/single_release_callback_impl.h"
#include "cc/resources/transferable_resource.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/resource_format.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "components/viz/common/resources/resource_settings.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
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
class SharedBitmap;
class SharedBitmapManager;
}  // namespace viz

namespace cc {
class BlockingTaskRunner;
class TextureIdAllocator;

// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT ResourceProvider
    : public base::trace_event::MemoryDumpProvider {
 private:
  struct Resource;

 public:
  using ResourceIdArray = std::vector<ResourceId>;
  using ResourceIdMap = std::unordered_map<ResourceId, ResourceId>;
  enum TextureHint {
    TEXTURE_HINT_DEFAULT = 0x0,
    TEXTURE_HINT_IMMUTABLE = 0x1,
    TEXTURE_HINT_FRAMEBUFFER = 0x2,
    TEXTURE_HINT_IMMUTABLE_FRAMEBUFFER =
        TEXTURE_HINT_IMMUTABLE | TEXTURE_HINT_FRAMEBUFFER
  };
  enum ResourceType {
    RESOURCE_TYPE_GPU_MEMORY_BUFFER,
    RESOURCE_TYPE_GL_TEXTURE,
    RESOURCE_TYPE_BITMAP,
  };

  ResourceProvider(viz::ContextProvider* compositor_context_provider,
                   viz::SharedBitmapManager* shared_bitmap_manager,
                   gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                   BlockingTaskRunner* blocking_main_thread_task_runner,
                   bool delegated_sync_points_required,
                   bool enable_color_correct_rasterization,
                   const viz::ResourceSettings& resource_settings);
  ~ResourceProvider() override;

  void Initialize();

  void DidLoseVulkanContextProvider() { lost_context_provider_ = true; }

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

  // Checks whether a resource is in use by a consumer.
  bool InUseByConsumer(ResourceId id);

  bool IsLost(ResourceId id);

  void LoseResourceForTesting(ResourceId id);
  void EnableReadLockFencesForTesting(ResourceId id);

  // Producer interface.

  ResourceType default_resource_type() const {
    return settings_.default_resource_type;
  }
  ResourceType GetResourceType(ResourceId id);
  GLenum GetResourceTextureTarget(ResourceId id);
  bool IsImmutable(ResourceId id);
  TextureHint GetTextureHint(ResourceId id);

  // Creates a resource of the default resource type.
  ResourceId CreateResource(const gfx::Size& size,
                            TextureHint hint,
                            viz::ResourceFormat format,
                            const gfx::ColorSpace& color_space);

  // Creates a resource for a particular texture target (the distinction between
  // texture targets has no effect in software mode).
  ResourceId CreateGpuMemoryBufferResource(const gfx::Size& size,
                                           TextureHint hint,
                                           viz::ResourceFormat format,
                                           gfx::BufferUsage usage,
                                           const gfx::ColorSpace& color_space);

  // Wraps an external texture mailbox into a GL resource.
  ResourceId CreateResourceFromTextureMailbox(
      const viz::TextureMailbox& mailbox,
      std::unique_ptr<SingleReleaseCallbackImpl> release_callback_impl);

  ResourceId CreateResourceFromTextureMailbox(
      const viz::TextureMailbox& mailbox,
      std::unique_ptr<SingleReleaseCallbackImpl> release_callback_impl,
      bool read_lock_fences_enabled);

  void DeleteResource(ResourceId id);
  // In the case of GPU resources, we may need to flush the GL context to ensure
  // that texture deletions are seen in a timely fashion. This function should
  // be called after texture deletions that may happen during an idle state.
  void FlushPendingDeletions() const;

  // Update pixels from image, copying source_rect (in image) to dest_offset (in
  // the resource).
  void CopyToResource(ResourceId id,
                      const uint8_t* image,
                      const gfx::Size& image_size);

  // Generates sync tokesn for resources which need a sync token.
  void GenerateSyncTokenForResource(ResourceId resource_id);
  void GenerateSyncTokenForResources(const ResourceIdArray& resource_ids);

  // Gets the most recent sync token from the indicated resources.
  gpu::SyncToken GetSyncTokenForResources(const ResourceIdArray& resource_ids);

  // Creates accounting for a child. Returns a child ID.
  int CreateChild(const ReturnCallback& return_callback);

  // Destroys accounting for the child, deleting all accounted resources.
  void DestroyChild(int child);

  // Sets whether resources need sync points set on them when returned to this
  // child. Defaults to true.
  void SetChildNeedsSyncTokens(int child, bool needs_sync_tokens);

  // Gets the child->parent resource ID map.
  const ResourceIdMap& GetChildToParentMap(int child) const;

  // Prepares resources to be transfered to the parent, moving them to
  // mailboxes and serializing meta-data into TransferableResources.
  // Resources are not removed from the ResourceProvider, but are marked as
  // "in use".
  void PrepareSendToParent(
      const ResourceIdArray& resource_ids,
      std::vector<TransferableResource>* transferable_resources);

  // Receives resources from a child, moving them from mailboxes. Resource IDs
  // passed are in the child namespace, and will be translated to the parent
  // namespace, added to the child->parent map.
  // This adds the resources to the working set in the ResourceProvider without
  // declaring which resources are in use. Use DeclareUsedResourcesFromChild
  // after calling this method to do that. All calls to ReceiveFromChild should
  // be followed by a DeclareUsedResourcesFromChild.
  // NOTE: if the sync_token is set on any TransferableResource, this will
  // wait on it.
  void ReceiveFromChild(
      int child,
      const std::vector<TransferableResource>& transferable_resources);

  // Once a set of resources have been received, they may or may not be used.
  // This declares what set of resources are currently in use from the child,
  // releasing any other resources back to the child.
  void DeclareUsedResourcesFromChild(int child,
                                     const ResourceIdSet& resources_from_child);

  // Receives resources from the parent, moving them from mailboxes. Resource
  // IDs passed are in the child namespace.
  // NOTE: if the sync_token is set on any TransferableResource, this will
  // wait on it.
  void ReceiveReturnsFromParent(
      const std::vector<ReturnedResource>& transferable_resources);

#if defined(OS_ANDROID)
  // Send an overlay promotion hint to all resources that requested it via
  // |wants_promotion_hints_set_|.  |promotable_hints| contains all the
  // resources that should be told that they're promotable.  Others will be told
  // that they're not promotable right now.
  void SendPromotionHints(
      const OverlayCandidateList::PromotionHintInfoMap& promotion_hints);
#endif

  // The following lock classes are part of the ResourceProvider API and are
  // needed to read and write the resource contents. The user must ensure
  // that they only use GL locks on GL resources, etc, and this is enforced
  // by assertions.
  class CC_EXPORT ScopedReadLockGL {
   public:
    ScopedReadLockGL(ResourceProvider* resource_provider,
                     ResourceId resource_id);
    ~ScopedReadLockGL();

    unsigned texture_id() const { return texture_id_; }
    GLenum target() const { return target_; }
    const gfx::Size& size() const { return size_; }
    const gfx::ColorSpace& color_space() const { return color_space_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    unsigned texture_id_;
    GLenum target_;
    gfx::Size size_;
    gfx::ColorSpace color_space_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockGL);
  };

  class CC_EXPORT ScopedSamplerGL {
   public:
    ScopedSamplerGL(ResourceProvider* resource_provider,
                    ResourceId resource_id,
                    GLenum filter);
    ScopedSamplerGL(ResourceProvider* resource_provider,
                    ResourceId resource_id,
                    GLenum unit,
                    GLenum filter);
    ~ScopedSamplerGL();

    unsigned texture_id() const { return resource_lock_.texture_id(); }
    GLenum target() const { return target_; }
    const gfx::ColorSpace& color_space() const {
      return resource_lock_.color_space();
    }

   private:
    ScopedReadLockGL resource_lock_;
    GLenum unit_;
    GLenum target_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSamplerGL);
  };

  class CC_EXPORT ScopedWriteLockGL {
   public:
    ScopedWriteLockGL(ResourceProvider* resource_provider,
                      ResourceId resource_id,
                      bool create_mailbox);
    ~ScopedWriteLockGL();

    unsigned texture_id() const { return texture_id_; }
    GLenum target() const { return target_; }
    viz::ResourceFormat format() const { return format_; }
    const gfx::Size& size() const { return size_; }
    // Will return the invalid color space unless
    // |enable_color_correct_rasterization| is true.
    const gfx::ColorSpace& color_space_for_raster() const {
      return color_space_;
    }

    const viz::TextureMailbox& mailbox() const { return mailbox_; }

    void set_sync_token(const gpu::SyncToken& sync_token) {
      sync_token_ = sync_token;
      has_sync_token_ = true;
    }

    void set_synchronized(bool synchronized) { synchronized_ = synchronized; }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    unsigned texture_id_;
    GLenum target_;
    viz::ResourceFormat format_;
    gfx::Size size_;
    viz::TextureMailbox mailbox_;
    gpu::SyncToken sync_token_;
    bool has_sync_token_;
    bool synchronized_;
    base::ThreadChecker thread_checker_;
    gfx::ColorSpace color_space_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGL);
  };

  class CC_EXPORT ScopedTextureProvider {
   public:
    ScopedTextureProvider(gpu::gles2::GLES2Interface* gl,
                          ScopedWriteLockGL* resource_lock,
                          bool use_mailbox);
    ~ScopedTextureProvider();

    unsigned texture_id() const { return texture_id_; }

   private:
    gpu::gles2::GLES2Interface* gl_;
    bool use_mailbox_;
    unsigned texture_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedTextureProvider);
  };

  class CC_EXPORT ScopedSkSurfaceProvider {
   public:
    ScopedSkSurfaceProvider(viz::ContextProvider* context_provider,
                            ScopedWriteLockGL* resource_lock,
                            bool use_mailbox,
                            bool use_distance_field_text,
                            bool can_use_lcd_text,
                            int msaa_sample_count);
    ~ScopedSkSurfaceProvider();

    SkSurface* sk_surface() { return sk_surface_.get(); }

   private:
    ScopedTextureProvider texture_provider_;
    sk_sp<SkSurface> sk_surface_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSkSurfaceProvider);
  };

  class CC_EXPORT ScopedReadLockSoftware {
   public:
    ScopedReadLockSoftware(ResourceProvider* resource_provider,
                           ResourceId resource_id);
    ~ScopedReadLockSoftware();

    const SkBitmap* sk_bitmap() const {
      DCHECK(valid());
      return &sk_bitmap_;
    }

    bool valid() const { return !!sk_bitmap_.getPixels(); }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    SkBitmap sk_bitmap_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSoftware);
  };

  class CC_EXPORT ScopedReadLockSkImage {
   public:
    ScopedReadLockSkImage(ResourceProvider* resource_provider,
                          ResourceId resource_id);
    ~ScopedReadLockSkImage();

    const SkImage* sk_image() const { return sk_image_.get(); }

    bool valid() const { return !!sk_image_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    sk_sp<SkImage> sk_image_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSkImage);
  };

  class CC_EXPORT ScopedWriteLockSoftware {
   public:
    ScopedWriteLockSoftware(ResourceProvider* resource_provider,
                            ResourceId resource_id);
    ~ScopedWriteLockSoftware();

    SkBitmap& sk_bitmap() { return sk_bitmap_; }
    bool valid() const { return !!sk_bitmap_.getPixels(); }
    // Will return the invalid color space unless
    // |enable_color_correct_rasterization| is true.
    const gfx::ColorSpace& color_space_for_raster() const {
      return color_space_;
    }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    SkBitmap sk_bitmap_;
    gfx::ColorSpace color_space_;
    base::ThreadChecker thread_checker_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockSoftware);
  };

  class CC_EXPORT ScopedWriteLockGpuMemoryBuffer {
   public:
    ScopedWriteLockGpuMemoryBuffer(ResourceProvider* resource_provider,
                                   ResourceId resource_id);
    ~ScopedWriteLockGpuMemoryBuffer();
    gfx::GpuMemoryBuffer* GetGpuMemoryBuffer();
    // Will return the invalid color space unless
    // |enable_color_correct_rasterization| is true.
    const gfx::ColorSpace& color_space_for_raster() const {
      return color_space_;
    }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    viz::ResourceFormat format_;
    gfx::BufferUsage usage_;
    gfx::Size size_;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
    gfx::ColorSpace color_space_;
    base::ThreadChecker thread_checker_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGpuMemoryBuffer);
  };

  // All resources that are returned to children while an instance of this
  // class exists will be stored and returned when the instance is destroyed.
  class CC_EXPORT ScopedBatchReturnResources {
   public:
    explicit ScopedBatchReturnResources(ResourceProvider* resource_provider);
    ~ScopedBatchReturnResources();

   private:
    ResourceProvider* resource_provider_;

    DISALLOW_COPY_AND_ASSIGN(ScopedBatchReturnResources);
  };

  class Fence : public base::RefCounted<Fence> {
   public:
    Fence() {}

    virtual void Set() = 0;
    virtual bool HasPassed() = 0;
    virtual void Wait() = 0;

   protected:
    friend class base::RefCounted<Fence>;
    virtual ~Fence() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Fence);
  };

  class SynchronousFence : public ResourceProvider::Fence {
   public:
    explicit SynchronousFence(gpu::gles2::GLES2Interface* gl);

    // Overridden from Fence:
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
  void AllocateForTesting(ResourceId id);

  // For tests only!
  void CreateForTesting(ResourceId id);

  // Sets the current read fence. If a resource is locked for read
  // and has read fences enabled, the resource will not allow writes
  // until this fence has passed.
  void SetReadLockFence(Fence* fence) { current_read_lock_fence_ = fence; }

  // Indicates if we can currently lock this resource for write.
  bool CanLockForWrite(ResourceId id);

  // Indicates if this resource may be used for a hardware overlay plane.
  bool IsOverlayCandidate(ResourceId id);

  // Return the format of the underlying buffer that can be used for scanout.
  gfx::BufferFormat GetBufferFormat(ResourceId id);

#if defined(OS_ANDROID)
  // Indicates if this resource is backed by an Android SurfaceTexture, and thus
  // can't really be promoted to an overlay.
  bool IsBackedBySurfaceTexture(ResourceId id);

  // Indicates if this resource wants to receive promotion hints.
  bool WantsPromotionHint(ResourceId id);

  // Return the number of resources that request promotion hints.
  size_t CountPromotionHintRequestsForTesting();
#endif

  void WaitSyncTokenIfNeeded(ResourceId id);

  static GLint GetActiveTextureUnit(gpu::gles2::GLES2Interface* gl);

  void ValidateResource(ResourceId id) const;

  GLenum GetImageTextureTarget(gfx::BufferUsage usage,
                               viz::ResourceFormat format);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  int tracing_id() const { return tracing_id_; }

 private:
  friend class ScopedBatchReturnResources;

  struct Resource {
    enum Origin { INTERNAL, EXTERNAL, DELEGATED };
    enum SynchronizationState {
      // The LOCALLY_USED state is the state each resource defaults to when
      // constructed or modified or read. This state indicates that the
      // resource has not been properly synchronized and it would be an error
      // to send this resource to a parent, child, or client.
      LOCALLY_USED,

      // The NEEDS_WAIT state is the state that indicates a resource has been
      // modified but it also has an associated sync token assigned to it.
      // The sync token has not been waited on with the local context. When
      // a sync token arrives from an external resource (such as a child or
      // parent), it is automatically initialized as NEEDS_WAIT as well
      // since we still need to wait on it before the resource is synchronized
      // on the current context. It is an error to use the resource locally for
      // reading or writing if the resource is in this state.
      NEEDS_WAIT,

      // The SYNCHRONIZED state indicates that the resource has been properly
      // synchronized locally. This can either synchronized externally (such
      // as the case of software rasterized bitmaps), or synchronized
      // internally using a sync token that has been waited upon. In the
      // former case where the resource was synchronized externally, a
      // corresponding sync token will not exist. In the latter case which was
      // synchronized from the NEEDS_WAIT state, a corresponding sync token will
      // exist which is assocaited with the resource. This sync token is still
      // valid and still associated with the resource and can be passed as an
      // external resource for others to wait on.
      SYNCHRONIZED,
    };

    ~Resource();
    Resource(unsigned texture_id,
             const gfx::Size& size,
             Origin origin,
             GLenum target,
             GLenum filter,
             TextureHint hint,
             ResourceType type,
             viz::ResourceFormat format);
    Resource(uint8_t* pixels,
             viz::SharedBitmap* bitmap,
             const gfx::Size& size,
             Origin origin,
             GLenum filter);
    Resource(const viz::SharedBitmapId& bitmap_id,
             const gfx::Size& size,
             Origin origin,
             GLenum filter);
    Resource(Resource&& other);

    bool needs_sync_token() const { return needs_sync_token_; }

    SynchronizationState synchronization_state() const {
      return synchronization_state_;
    }

    const viz::TextureMailbox& mailbox() const { return mailbox_; }
    void set_mailbox(const viz::TextureMailbox& mailbox);

    void SetLocallyUsed();
    void SetSynchronized();
    void UpdateSyncToken(const gpu::SyncToken& sync_token);
    int8_t* GetSyncTokenData();
    void WaitSyncToken(gpu::gles2::GLES2Interface* gl);

    int child_id;
    ResourceId id_in_child;
    unsigned gl_id;
    // Pixel buffer used for set pixels without unnecessary copying.
    unsigned gl_pixel_buffer_id;
    // Query used to determine when asynchronous set pixels complete.
    unsigned gl_upload_query_id;
    // Query used to determine when read lock fence has passed.
    unsigned gl_read_lock_query_id;
    ReleaseCallbackImpl release_callback_impl;
    uint8_t* pixels;
    int lock_for_read_count;
    int imported_count;
    int exported_count;
    bool dirty_image : 1;
    bool locked_for_write : 1;
    bool lost : 1;
    bool marked_for_deletion : 1;
    bool allocated : 1;
    bool read_lock_fences_enabled : 1;
    bool has_shared_bitmap_id : 1;
    bool is_overlay_candidate : 1;
#if defined(OS_ANDROID)
    // Indicates whether this resource may not be overlayed on Android, since
    // it's not backed by a SurfaceView.  This may be set in combination with
    // |is_overlay_candidate|, to find out if switching the resource to a
    // a SurfaceView would result in overlay promotion.  It's good to find this
    // out in advance, since one has no fallback path for displaying a
    // SurfaceView except via promoting it to an overlay.  Ideally, one _could_
    // promote SurfaceTexture via the overlay path, even if one ended up just
    // drawing a quad in the compositor.  However, for now, we use this flag to
    // refuse to promote so that the compositor will draw the quad.
    bool is_backed_by_surface_texture : 1;
    // Indicates that this resource would like a promotion hint.
    bool wants_promotion_hint : 1;
#endif
    scoped_refptr<Fence> read_lock_fence;
    gfx::Size size;
    Origin origin;
    GLenum target;
    // TODO(skyostil): Use a separate sampler object for filter state.
    GLenum original_filter;
    GLenum filter;
    unsigned image_id;
    unsigned bound_image_id;
    TextureHint hint;
    ResourceType type;

    // GpuMemoryBuffer resource allocation needs to know how the resource will
    // be used.
    gfx::BufferUsage usage;
    // This is the the actual format of the underlaying GpuMemoryBuffer, if any,
    // and might not correspond to viz::ResourceFormat. This format is needed to
    // scanout the buffer as HW overlay.
    gfx::BufferFormat buffer_format;
    // Resource format is the format as seen from the compositor and might not
    // correspond to buffer_format (e.g: A resouce that was created from a YUV
    // buffer could be seen as RGB from the compositor/GL.)
    viz::ResourceFormat format;
    viz::SharedBitmapId shared_bitmap_id;
    viz::SharedBitmap* shared_bitmap;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
    gfx::ColorSpace color_space;

   private:
    SynchronizationState synchronization_state_ = SYNCHRONIZED;
    bool needs_sync_token_ = false;
    viz::TextureMailbox mailbox_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };
  using ResourceMap = std::unordered_map<ResourceId, Resource>;

  struct Child {
    Child();
    Child(const Child& other);
    ~Child();

    ResourceIdMap child_to_parent_map;
    ReturnCallback return_callback;
    bool marked_for_deletion;
    bool needs_sync_tokens;
  };
  using ChildMap = std::unordered_map<int, Child>;

  bool ReadLockFenceHasPassed(const Resource* resource) {
    return !resource->read_lock_fence.get() ||
           resource->read_lock_fence->HasPassed();
  }

  ResourceId CreateGLTexture(const gfx::Size& size,
                             TextureHint hint,
                             ResourceType type,
                             viz::ResourceFormat format,
                             gfx::BufferUsage usage,
                             const gfx::ColorSpace& color_space);
  ResourceId CreateBitmap(const gfx::Size& size,
                          const gfx::ColorSpace& color_space);
  Resource* InsertResource(ResourceId id, Resource resource);
  Resource* GetResource(ResourceId id);
  const Resource* LockForRead(ResourceId id);
  void UnlockForRead(ResourceId id);
  Resource* LockForWrite(ResourceId id);
  void UnlockForWrite(Resource* resource);

  void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                    const Resource* resource);

  void CreateMailboxAndBindResource(gpu::gles2::GLES2Interface* gl,
                                    Resource* resource);

  void TransferResource(Resource* source,
                        ResourceId id,
                        TransferableResource* resource);
  enum DeleteStyle {
    NORMAL,
    FOR_SHUTDOWN,
  };
  void DeleteResourceInternal(ResourceMap::iterator it, DeleteStyle style);
  void DeleteAndReturnUnusedResourcesToChild(ChildMap::iterator child_it,
                                             DeleteStyle style,
                                             const ResourceIdArray& unused);
  void DestroyChildInternal(ChildMap::iterator it, DeleteStyle style);
  void LazyCreate(Resource* resource);
  void LazyAllocate(Resource* resource);
  void LazyCreateImage(Resource* resource);

  void BindImageForSampling(Resource* resource);
  // Binds the given GL resource to a texture target for sampling using the
  // specified filter for both minification and magnification. Returns the
  // texture target used. The resource must be locked for reading.
  GLenum BindForSampling(ResourceId resource_id, GLenum unit, GLenum filter);

  // Returns null if we do not have a viz::ContextProvider.
  gpu::gles2::GLES2Interface* ContextGL() const;
  bool IsGLContextLost() const;

  // Will return the invalid color space unless
  // |enable_color_correct_rasterization| is true.
  gfx::ColorSpace GetResourceColorSpaceForRaster(
      const Resource* resource) const;

  void SetBatchReturnResources(bool aggregate);

  // Holds const settings for the ResourceProvider. Never changed after init.
  struct Settings {
    Settings(viz::ContextProvider* compositor_context_provider,
             bool delegated_sync_points_needed,
             bool enable_color_correct_rasterization,
             const viz::ResourceSettings& resource_settings);

    int max_texture_size = 0;
    bool use_texture_storage_ext = false;
    bool use_texture_format_bgra = false;
    bool use_texture_usage_hint = false;
    bool use_sync_query = false;
    ResourceType default_resource_type = RESOURCE_TYPE_GL_TEXTURE;
    viz::ResourceFormat yuv_resource_format = viz::LUMINANCE_8;
    viz::ResourceFormat yuv_highbit_resource_format = viz::LUMINANCE_8;
    viz::ResourceFormat best_texture_format = viz::RGBA_8888;
    viz::ResourceFormat best_render_buffer_format = viz::RGBA_8888;
    bool enable_color_correct_rasterization = false;
    bool delegated_sync_points_required = false;
  } const settings_;

  viz::ContextProvider* compositor_context_provider_;
  viz::SharedBitmapManager* shared_bitmap_manager_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  BlockingTaskRunner* blocking_main_thread_task_runner_;
  bool lost_context_provider_;
  ResourceId next_id_;
  ResourceMap resources_;
  int next_child_;
  ChildMap children_;
  scoped_refptr<Fence> current_read_lock_fence_;
  std::unique_ptr<TextureIdAllocator> texture_id_allocator_;
  viz::BufferToTextureTargetMap buffer_to_texture_target_map_;

  // Keep track of whether deleted resources should be batched up or returned
  // immediately.
  bool batch_return_resources_ = false;
  // Maps from a child id to the set of resources to be returned to it.
  base::small_map<std::map<int, ResourceIdArray>> batched_returning_resources_;

  base::ThreadChecker thread_checker_;

  // A process-unique ID used for disambiguating memory dumps from different
  // resource providers.
  int tracing_id_;

#if defined(OS_ANDROID)
  // Set of resource Ids that would like to be notified about promotion hints.
  ResourceIdSet wants_promotion_hints_set_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ResourceProvider);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_PROVIDER_H_

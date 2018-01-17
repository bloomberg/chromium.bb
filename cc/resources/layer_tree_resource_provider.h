// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_LAYER_TREE_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_LAYER_TREE_RESOURCE_PROVIDER_H_

#include "cc/resources/resource_provider.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/resources/resource_settings.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace viz {
class SharedBitmapManager;
}  // namespace viz

namespace gpu {
struct Capabilities;
class GpuMemoryBufferManager;
namespace raster {
class RasterInterface;
}
}  // namespace gpu

namespace cc {
class TextureIdAllocator;

// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT LayerTreeResourceProvider : public ResourceProvider {
 public:
  LayerTreeResourceProvider(
      viz::ContextProvider* compositor_context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      bool delegated_sync_points_required,
      const viz::ResourceSettings& resource_settings);
  ~LayerTreeResourceProvider() override;

  static gpu::SyncToken GenerateSyncTokenHelper(gpu::gles2::GLES2Interface* gl);
  static gpu::SyncToken GenerateSyncTokenHelper(
      gpu::raster::RasterInterface* ri);

  // Gets the most recent sync token from the indicated resources.
  gpu::SyncToken GetSyncTokenForResources(const ResourceIdArray& resource_ids);

  // Prepares resources to be transfered to the parent, moving them to
  // mailboxes and serializing meta-data into TransferableResources.
  // Resources are not removed from the ResourceProvider, but are marked as
  // "in use".
  void PrepareSendToParent(
      const ResourceIdArray& resource_ids,
      std::vector<viz::TransferableResource>* transferable_resources);

  // Receives resources from the parent, moving them from mailboxes. ResourceIds
  // passed are in the child namespace.
  // NOTE: if the sync_token is set on any viz::TransferableResource, this will
  // wait on it.
  void ReceiveReturnsFromParent(
      const std::vector<viz::ReturnedResource>& transferable_resources);

  viz::ResourceId CreateGpuTextureResource(const gfx::Size& size,
                                           viz::ResourceTextureHint hint,
                                           viz::ResourceFormat format,
                                           const gfx::ColorSpace& color_space);
  viz::ResourceId CreateGpuMemoryBufferResource(
      const gfx::Size& size,
      viz::ResourceTextureHint hint,
      viz::ResourceFormat format,
      gfx::BufferUsage usage,
      const gfx::ColorSpace& color_space);
  viz::ResourceId CreateBitmapResource(const gfx::Size& size,
                                       const gfx::ColorSpace& color_space);

  void DeleteResource(viz::ResourceId id);

  // Receives a resource from an external client that can be used in compositor
  // frames, via the returned ResourceId.
  viz::ResourceId ImportResource(const viz::TransferableResource&,
                                 std::unique_ptr<viz::SingleReleaseCallback>);
  // Removes an imported resource, which will call the ReleaseCallback given
  // originally, once the resource is no longer in use by any compositor frame.
  void RemoveImportedResource(viz::ResourceId);
  // Update pixels from image, copying source_rect (in image) to dest_offset (in
  // the resource).
  void CopyToResource(viz::ResourceId id,
                      const uint8_t* image,
                      const gfx::Size& image_size);

  // For tests only! This prevents detecting uninitialized reads.
  // Use SetPixels or LockForWrite to allocate implicitly.
  void AllocateForTesting(viz::ResourceId id);

  // For tests only!
  void CreateForTesting(viz::ResourceId id);

  // Verify that the ResourceId is valid and is known to this class, for debug
  // checks.
  void ValidateResource(viz::ResourceId id) const;

  // Checks whether a resource is in use by a consumer.
  bool InUseByConsumer(viz::ResourceId id);

  // Indicates if we can currently lock this resource for write.
  bool CanLockForWrite(viz::ResourceId id);

  // In the case of GPU resources, we may need to flush the GL context to ensure
  // that texture deletions are seen in a timely fashion. This function should
  // be called after texture deletions that may happen during an idle state.
  void FlushPendingDeletions() const;

  GLenum GetImageTextureTarget(const gpu::Capabilities& caps,
                               gfx::BufferUsage usage,
                               viz::ResourceFormat format) const;

  bool IsTextureFormatSupported(viz::ResourceFormat format) const;

  // Returns true if the provided |format| can be used as a render buffer.
  // Note that render buffer support implies texture support.
  bool IsRenderBufferFormatSupported(viz::ResourceFormat format) const;

  bool IsGpuMemoryBufferFormatSupported(viz::ResourceFormat format,
                                        gfx::BufferUsage usage) const;

  bool use_sync_query() const { return settings_.use_sync_query; }

  int max_texture_size() const { return settings_.max_texture_size; }

  // Use this format for making resources that will be rastered and uploaded to
  // from software bitmaps.
  viz::ResourceFormat best_texture_format() const {
    return settings_.best_texture_format;
  }
  // Use this format for making resources that will be rastered to on the Gpu.
  viz::ResourceFormat best_render_buffer_format() const {
    return settings_.best_render_buffer_format;
  }

  viz::ResourceFormat YuvResourceFormat(int bits) const;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() {
    return gpu_memory_buffer_manager_;
  }

  // The following lock classes are part of the ResourceProvider API and are
  // needed to read and write the resource contents. The user must ensure
  // that they only use GL locks on GL resources, etc, and this is enforced
  // by assertions.
  class CC_EXPORT ScopedWriteLockGpu {
   public:
    ScopedWriteLockGpu(LayerTreeResourceProvider* resource_provider,
                       viz::ResourceId resource_id);
    ~ScopedWriteLockGpu();

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

    // Creates mailbox on compositor context that can be consumed on another
    // context.
    void CreateMailbox();

   protected:
    LayerTreeResourceProvider* const resource_provider_;
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

   private:
    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGpu);
  };

  class CC_EXPORT ScopedWriteLockGL : public ScopedWriteLockGpu {
   public:
    ScopedWriteLockGL(LayerTreeResourceProvider* resource_provider,
                      viz::ResourceId resource_id);
    ~ScopedWriteLockGL();

    // Returns texture id on compositor context, allocating if necessary.
    GLuint GetTexture();

   private:
    void LazyAllocate(gpu::gles2::GLES2Interface* gl, GLuint texture_id);

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGL);
  };

  class CC_EXPORT ScopedWriteLockRaster : public ScopedWriteLockGpu {
   public:
    ScopedWriteLockRaster(LayerTreeResourceProvider* resource_provider,
                          viz::ResourceId resource_id);
    ~ScopedWriteLockRaster();

    // Creates a texture id, allocating if necessary, on the given context. The
    // texture id must be deleted by the caller.
    GLuint ConsumeTexture(gpu::raster::RasterInterface* ri);

   private:
    void LazyAllocate(gpu::raster::RasterInterface* gl, GLuint texture_id);

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockRaster);
  };

  class CC_EXPORT ScopedWriteLockGpuMemoryBuffer {
   public:
    ScopedWriteLockGpuMemoryBuffer(LayerTreeResourceProvider* resource_provider,
                                   viz::ResourceId resource_id);
    ~ScopedWriteLockGpuMemoryBuffer();
    gfx::GpuMemoryBuffer* GetGpuMemoryBuffer();
    const gfx::ColorSpace& color_space_for_raster() const {
      return color_space_;
    }

   private:
    LayerTreeResourceProvider* const resource_provider_;
    const viz::ResourceId resource_id_;

    gfx::Size size_;
    viz::ResourceFormat format_;
    gfx::BufferUsage usage_;
    gfx::ColorSpace color_space_;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGpuMemoryBuffer);
  };

  class CC_EXPORT ScopedWriteLockSoftware {
   public:
    ScopedWriteLockSoftware(LayerTreeResourceProvider* resource_provider,
                            viz::ResourceId resource_id);
    ~ScopedWriteLockSoftware();

    SkBitmap& sk_bitmap() { return sk_bitmap_; }
    bool valid() const { return !!sk_bitmap_.getPixels(); }
    const gfx::ColorSpace& color_space_for_raster() const {
      return color_space_;
    }

   private:
    LayerTreeResourceProvider* const resource_provider_;
    const viz::ResourceId resource_id_;
    gfx::ColorSpace color_space_;
    SkBitmap sk_bitmap_;
    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockSoftware);
  };

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

 protected:
  viz::internal::Resource* LockForWrite(viz::ResourceId id);
  void UnlockForWrite(viz::internal::Resource* resource);
  void CreateMailbox(viz::internal::Resource* resource);

 private:
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

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  gfx::ColorSpace GetResourceColorSpaceForRaster(
      const viz::internal::Resource* resource) const;

  void CreateTexture(viz::internal::Resource* resource);
  void CreateAndBindImage(viz::internal::Resource* resource);
  void TransferResource(viz::internal::Resource* source,
                        viz::ResourceId id,
                        viz::TransferableResource* resource);

  viz::SharedBitmapManager* shared_bitmap_manager_;
  struct ImportedResource;
  base::flat_map<viz::ResourceId, ImportedResource> imported_resources_;
  std::unique_ptr<TextureIdAllocator> texture_id_allocator_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  viz::ResourceId next_id_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeResourceProvider);
};

}  // namespace cc

#endif  // CC_RESOURCES_LAYER_TREE_RESOURCE_PROVIDER_H_

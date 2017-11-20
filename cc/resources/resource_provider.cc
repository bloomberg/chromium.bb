// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <unordered_map>

#include "base/atomic_sequence_num.h"
#include "base/bits.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/resources/resource_util.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "skia/ext/texture_handle.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gl/trace_util.h"

using gpu::gles2::GLES2Interface;

namespace cc {

class TextureIdAllocator {
 public:
  TextureIdAllocator(GLES2Interface* gl,
                     size_t texture_id_allocation_chunk_size)
      : gl_(gl),
        id_allocation_chunk_size_(texture_id_allocation_chunk_size),
        ids_(new GLuint[texture_id_allocation_chunk_size]),
        next_id_index_(texture_id_allocation_chunk_size) {
    DCHECK(id_allocation_chunk_size_);
    DCHECK_LE(id_allocation_chunk_size_,
              static_cast<size_t>(std::numeric_limits<int>::max()));
  }

  ~TextureIdAllocator() {
    gl_->DeleteTextures(
        static_cast<int>(id_allocation_chunk_size_ - next_id_index_),
        ids_.get() + next_id_index_);
  }

  GLuint NextId() {
    if (next_id_index_ == id_allocation_chunk_size_) {
      gl_->GenTextures(static_cast<int>(id_allocation_chunk_size_), ids_.get());
      next_id_index_ = 0;
    }

    return ids_[next_id_index_++];
  }

 private:
  GLES2Interface* gl_;
  const size_t id_allocation_chunk_size_;
  std::unique_ptr<GLuint[]> ids_;
  size_t next_id_index_;

  DISALLOW_COPY_AND_ASSIGN(TextureIdAllocator);
};

namespace {

class ScopedSetActiveTexture {
 public:
  ScopedSetActiveTexture(GLES2Interface* gl, GLenum unit)
      : gl_(gl), unit_(unit) {
    DCHECK_EQ(GL_TEXTURE0, ResourceProvider::GetActiveTextureUnit(gl_));

    if (unit_ != GL_TEXTURE0)
      gl_->ActiveTexture(unit_);
  }

  ~ScopedSetActiveTexture() {
    // Active unit being GL_TEXTURE0 is effectively the ground state.
    if (unit_ != GL_TEXTURE0)
      gl_->ActiveTexture(GL_TEXTURE0);
  }

 private:
  GLES2Interface* gl_;
  GLenum unit_;
};

// Generates process-unique IDs to use for tracing a ResourceProvider's
// resources.
base::AtomicSequenceNumber g_next_resource_provider_tracing_id;

}  // namespace

ResourceProvider::Settings::Settings(
    viz::ContextProvider* compositor_context_provider,
    bool delegated_sync_points_required,
    const viz::ResourceSettings& resource_settings)
    : yuv_highbit_resource_format(resource_settings.high_bit_for_testing
                                      ? viz::R16_EXT
                                      : viz::LUMINANCE_8),
      use_gpu_memory_buffer_resources(
          resource_settings.use_gpu_memory_buffer_resources),
      delegated_sync_points_required(delegated_sync_points_required) {
  if (!compositor_context_provider) {
    // Pick an arbitrary limit here similar to what hardware might.
    max_texture_size = 16 * 1024;
    best_texture_format = viz::RGBA_8888;
    return;
  }

  const auto& caps = compositor_context_provider->ContextCapabilities();
  use_texture_storage = caps.texture_storage;
  use_texture_format_bgra = caps.texture_format_bgra8888;
  use_texture_usage_hint = caps.texture_usage;
  use_texture_npot = caps.texture_npot;
  use_sync_query = caps.sync_query;
  use_texture_storage_image = caps.texture_storage_image;

  if (caps.disable_one_component_textures) {
    yuv_resource_format = yuv_highbit_resource_format = viz::RGBA_8888;
  } else {
    yuv_resource_format = caps.texture_rg ? viz::RED_8 : viz::LUMINANCE_8;
    if (resource_settings.use_r16_texture && caps.texture_norm16)
      yuv_highbit_resource_format = viz::R16_EXT;
    else if (caps.texture_half_float_linear)
      yuv_highbit_resource_format = viz::LUMINANCE_F16;
  }

  GLES2Interface* gl = compositor_context_provider->ContextGL();
  gl->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

  best_texture_format =
      viz::PlatformColor::BestSupportedTextureFormat(use_texture_format_bgra);
  best_render_buffer_format = viz::PlatformColor::BestSupportedTextureFormat(
      caps.render_buffer_format_bgra8888);
}

ResourceProvider::ResourceProvider(
    viz::ContextProvider* compositor_context_provider,
    viz::SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool delegated_sync_points_required,
    const viz::ResourceSettings& resource_settings,
    viz::ResourceId next_id)
    : settings_(compositor_context_provider,
                delegated_sync_points_required,
                resource_settings),
      compositor_context_provider_(compositor_context_provider),
      shared_bitmap_manager_(shared_bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      next_id_(next_id),
      next_child_(1),
      lost_context_provider_(false),
      buffer_to_texture_target_map_(
          resource_settings.buffer_to_texture_target_map),
      tracing_id_(g_next_resource_provider_tracing_id.GetNext()) {
  DCHECK(resource_settings.texture_id_allocation_chunk_size);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(next_id_, viz::kInvalidResourceId);

  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  // TODO(ericrk): Get this working in Android Webview. crbug.com/517156
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::ResourceProvider", base::ThreadTaskRunnerHandle::Get());
  }

  if (!compositor_context_provider)
    return;

  DCHECK(!texture_id_allocator_);
  GLES2Interface* gl = ContextGL();
  texture_id_allocator_.reset(new TextureIdAllocator(
      gl, resource_settings.texture_id_allocation_chunk_size));
}

ResourceProvider::~ResourceProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  while (!resources_.empty())
    DeleteResourceInternal(resources_.begin(), FOR_SHUTDOWN);

  if (!compositor_context_provider_)
    return;

#if DCHECK_IS_ON()
  // Check that all GL resources has been deleted.
  for (const auto& pair : resources_)
    DCHECK(!pair.second.is_gpu_resource_type());
#endif  // DCHECK_IS_ON()

  texture_id_allocator_ = nullptr;

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  gl->Finish();
}

bool ResourceProvider::IsTextureFormatSupported(
    viz::ResourceFormat format) const {
  gpu::Capabilities caps;
  if (compositor_context_provider_)
    caps = compositor_context_provider_->ContextCapabilities();

  switch (format) {
    case viz::ALPHA_8:
    case viz::RGBA_4444:
    case viz::RGBA_8888:
    case viz::RGB_565:
    case viz::LUMINANCE_8:
      return true;
    case viz::BGRA_8888:
      return caps.texture_format_bgra8888;
    case viz::ETC1:
      return caps.texture_format_etc1;
    case viz::RED_8:
      return caps.texture_rg;
    case viz::R16_EXT:
      return caps.texture_norm16;
    case viz::LUMINANCE_F16:
    case viz::RGBA_F16:
      return caps.texture_half_float_linear;
  }

  NOTREACHED();
  return false;
}

bool ResourceProvider::IsRenderBufferFormatSupported(
    viz::ResourceFormat format) const {
  gpu::Capabilities caps;
  if (compositor_context_provider_)
    caps = compositor_context_provider_->ContextCapabilities();

  switch (format) {
    case viz::RGBA_4444:
    case viz::RGBA_8888:
    case viz::RGB_565:
      return true;
    case viz::BGRA_8888:
      return caps.render_buffer_format_bgra8888;
    case viz::RGBA_F16:
      // TODO(ccameron): This will always return false on pixel tests, which
      // makes it un-test-able until we upgrade Mesa.
      // https://crbug.com/687720
      return caps.texture_half_float_linear &&
             caps.color_buffer_half_float_rgba;
    case viz::LUMINANCE_8:
    case viz::ALPHA_8:
    case viz::RED_8:
    case viz::ETC1:
    case viz::LUMINANCE_F16:
    case viz::R16_EXT:
      // We don't currently render into these formats. If we need to render into
      // these eventually, we should expand this logic.
      return false;
  }

  NOTREACHED();
  return false;
}

bool ResourceProvider::IsGpuMemoryBufferFormatSupported(
    viz::ResourceFormat format,
    gfx::BufferUsage usage) const {
  switch (format) {
    case viz::BGRA_8888:
    case viz::RED_8:
    case viz::R16_EXT:
    case viz::RGBA_4444:
    case viz::RGBA_8888:
    case viz::ETC1:
    case viz::RGBA_F16:
      return buffer_to_texture_target_map_.find(
                 viz::BufferToTextureTargetKey(usage, BufferFormat(format))) !=
             buffer_to_texture_target_map_.end();
    // These formats have no BufferFormat equivalent.
    case viz::ALPHA_8:
    case viz::LUMINANCE_8:
    case viz::RGB_565:
    case viz::LUMINANCE_F16:
      return false;
  }
  NOTREACHED();
  return false;
}

bool ResourceProvider::InUseByConsumer(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->lock_for_read_count > 0 || resource->exported_count > 0 ||
         resource->lost;
}

bool ResourceProvider::IsLost(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->lost;
}

void ResourceProvider::LoseResourceForTesting(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  DCHECK(resource);
  resource->lost = true;
}

viz::ResourceFormat ResourceProvider::YuvResourceFormat(int bits) const {
  if (bits > 8) {
    return settings_.yuv_highbit_resource_format;
  } else {
    return settings_.yuv_resource_format;
  }
}

viz::ResourceId ResourceProvider::CreateResource(
    const gfx::Size& size,
    viz::ResourceTextureHint hint,
    viz::ResourceFormat format,
    const gfx::ColorSpace& color_space) {
  DCHECK(!size.IsEmpty());

  if (compositor_context_provider_)
    return CreateGpuTextureResource(size, hint, format, color_space);

  DCHECK_EQ(viz::RGBA_8888, format);
  return CreateBitmapResource(size, color_space);
}

viz::ResourceId ResourceProvider::CreateGpuMemoryBufferResource(
    const gfx::Size& size,
    viz::ResourceTextureHint hint,
    viz::ResourceFormat format,
    gfx::BufferUsage usage,
    const gfx::ColorSpace& color_space) {
  DCHECK(!size.IsEmpty());
  DCHECK(compositor_context_provider_);
  DCHECK_LE(size.width(), settings_.max_texture_size);
  DCHECK_LE(size.height(), settings_.max_texture_size);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  viz::ResourceId id = next_id_++;
  viz::internal::Resource* resource = InsertResource(
      id, viz::internal::Resource(size, viz::internal::Resource::INTERNAL, hint,
                                  viz::ResourceType::kGpuMemoryBuffer, format,
                                  color_space));
  resource->target = GetImageTextureTarget(usage, format);
  resource->buffer_format = BufferFormat(format);
  resource->usage = usage;
  resource->is_overlay_candidate = true;
  // GpuMemoryBuffer provides direct access to the memory used by the GPU. Read
  // lock fences are required to ensure that we're not trying to map a buffer
  // that is currently in-use by the GPU.
  resource->read_lock_fences_enabled = true;
  return id;
}

viz::ResourceId ResourceProvider::CreateGpuTextureResource(
    const gfx::Size& size,
    viz::ResourceTextureHint hint,
    viz::ResourceFormat format,
    const gfx::ColorSpace& color_space) {
  DCHECK_LE(size.width(), settings_.max_texture_size);
  DCHECK_LE(size.height(), settings_.max_texture_size);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsTextureFormatSupported(format));

  bool use_overlay = settings_.use_gpu_memory_buffer_resources ||
                     (hint & viz::ResourceTextureHint::kOverlay);
  DCHECK(!use_overlay || !(hint & viz::ResourceTextureHint::kMipmap));

  viz::ResourceId id = next_id_++;
  viz::internal::Resource* resource =
      InsertResource(id, viz::internal::Resource(
                             size, viz::internal::Resource::INTERNAL, hint,
                             viz::ResourceType::kTexture, format, color_space));
  if (use_overlay && settings_.use_texture_storage_image &&
      IsGpuMemoryBufferFormatSupported(format, gfx::BufferUsage::SCANOUT)) {
    resource->usage = gfx::BufferUsage::SCANOUT;
    resource->target = GetImageTextureTarget(resource->usage, format);
    resource->buffer_format = BufferFormat(format);
    resource->is_overlay_candidate = true;
  }
  return id;
}

viz::ResourceId ResourceProvider::CreateBitmapResource(
    const gfx::Size& size,
    const gfx::ColorSpace& color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<viz::SharedBitmap> bitmap =
      shared_bitmap_manager_->AllocateSharedBitmap(size);
  DCHECK(bitmap);
  DCHECK(bitmap->pixels());

  viz::ResourceId id = next_id_++;
  viz::internal::Resource* resource = InsertResource(
      id, viz::internal::Resource(size, viz::internal::Resource::INTERNAL,
                                  viz::ResourceTextureHint::kDefault,
                                  viz::ResourceType::kBitmap, viz::RGBA_8888,
                                  color_space));
  resource->SetSharedBitmap(bitmap.get());
  resource->owned_shared_bitmap = std::move(bitmap);
  return id;
}

void ResourceProvider::DeleteResource(viz::ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  viz::internal::Resource* resource = &it->second;
  DCHECK(!resource->marked_for_deletion);
  DCHECK_EQ(resource->imported_count, 0);
  DCHECK(!resource->locked_for_write);

  if (resource->exported_count > 0 || resource->lock_for_read_count > 0 ||
      !ReadLockFenceHasPassed(resource)) {
    resource->marked_for_deletion = true;
    return;
  } else {
    DeleteResourceInternal(it, NORMAL);
  }
}

void ResourceProvider::DeleteResourceInternal(ResourceMap::iterator it,
                                              DeleteStyle style) {
  TRACE_EVENT0("cc", "ResourceProvider::DeleteResourceInternal");
  viz::internal::Resource* resource = &it->second;
  DCHECK(resource->exported_count == 0 || style != NORMAL);

#if defined(OS_ANDROID)
  // If this resource was interested in promotion hints, then remove it from
  // the set of resources that we'll notify.
  if (resource->wants_promotion_hint)
    wants_promotion_hints_set_.erase(it->first);
#endif

  // Exported resources are lost on shutdown.
  bool exported_resource_lost =
      style == FOR_SHUTDOWN && resource->exported_count > 0;
  // GPU resources are lost when context is lost.
  bool gpu_resource_lost =
      resource->is_gpu_resource_type() && lost_context_provider_;
  bool lost_resource =
      resource->lost || exported_resource_lost || gpu_resource_lost;

  // Wait on sync token before deleting resources we own.
  if (!lost_resource && resource->origin == viz::internal::Resource::INTERNAL)
    WaitSyncTokenInternal(resource);

  if (resource->image_id) {
    DCHECK_EQ(resource->origin, viz::internal::Resource::INTERNAL);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DestroyImageCHROMIUM(resource->image_id);
  }

  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DeleteTextures(1, &resource->gl_id);
    resource->gl_id = 0;
  }

  if (resource->owned_shared_bitmap) {
    DCHECK_EQ(viz::ResourceType::kBitmap, resource->type);
    resource->shared_bitmap = nullptr;
    resource->pixels = nullptr;
    resource->owned_shared_bitmap = nullptr;
  }

  if (resource->gpu_memory_buffer) {
    DCHECK_EQ(viz::ResourceType::kGpuMemoryBuffer, resource->type);
    resource->gpu_memory_buffer = nullptr;
  }

  resources_.erase(it);
}

void ResourceProvider::FlushPendingDeletions() const {
  if (auto* gl = ContextGL())
    gl->ShallowFlushCHROMIUM();
}

viz::ResourceType ResourceProvider::GetResourceType(viz::ResourceId id) {
  return GetResource(id)->type;
}

GLenum ResourceProvider::GetResourceTextureTarget(viz::ResourceId id) {
  return GetResource(id)->target;
}

viz::ResourceTextureHint ResourceProvider::GetTextureHint(viz::ResourceId id) {
  return GetResource(id)->hint;
}

gfx::ColorSpace ResourceProvider::GetResourceColorSpaceForRaster(
    const viz::internal::Resource* resource) const {
  return resource->color_space;
}

void ResourceProvider::CopyToResource(viz::ResourceId id,
                                      const uint8_t* image,
                                      const gfx::Size& image_size) {
  viz::internal::Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write);
  DCHECK(!resource->lock_for_read_count);
  DCHECK_EQ(resource->origin, viz::internal::Resource::INTERNAL);
  DCHECK_NE(resource->synchronization_state(),
            viz::internal::Resource::NEEDS_WAIT);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(ReadLockFenceHasPassed(resource));

  DCHECK_EQ(image_size.width(), resource->size.width());
  DCHECK_EQ(image_size.height(), resource->size.height());

  if (resource->type == viz::ResourceType::kBitmap) {
    DCHECK_EQ(viz::ResourceType::kBitmap, resource->type);
    DCHECK(resource->allocated);
    DCHECK_EQ(viz::RGBA_8888, resource->format);
    SkImageInfo source_info =
        SkImageInfo::MakeN32Premul(image_size.width(), image_size.height());
    size_t image_stride = image_size.width() * 4;

    ScopedWriteLockSoftware lock(this, id);
    SkCanvas dest(lock.sk_bitmap());
    dest.writePixels(source_info, image, image_stride, 0, 0);
  } else {
    // No sync token needed because the lock will set synchronization state to
    // LOCALLY_USED and a sync token will be generated in PrepareSendToParent.
    ScopedWriteLockGL lock(this, id);
    GLuint texture_id = lock.GetTexture();
    DCHECK(texture_id);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->BindTexture(resource->target, texture_id);
    if (resource->format == viz::ETC1) {
      DCHECK_EQ(resource->target, static_cast<GLenum>(GL_TEXTURE_2D));
      int image_bytes =
          ResourceUtil::CheckedSizeInBytes<int>(image_size, viz::ETC1);
      gl->CompressedTexImage2D(resource->target, 0, GLInternalFormat(viz::ETC1),
                               image_size.width(), image_size.height(), 0,
                               image_bytes, image);
      lock.set_allocated();
    } else {
      gl->TexSubImage2D(resource->target, 0, 0, 0, image_size.width(),
                        image_size.height(), GLDataFormat(resource->format),
                        GLDataType(resource->format), image);
    }
  }
  DCHECK(resource->allocated);
}

viz::internal::Resource* ResourceProvider::InsertResource(
    viz::ResourceId id,
    viz::internal::Resource resource) {
  std::pair<ResourceMap::iterator, bool> result =
      resources_.insert(ResourceMap::value_type(id, std::move(resource)));
  DCHECK(result.second);
  return &result.first->second;
}

viz::internal::Resource* ResourceProvider::GetResource(viz::ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(id);
  ResourceMap::iterator it = resources_.find(id);
  DCHECK(it != resources_.end());
  return &it->second;
}

viz::internal::Resource* ResourceProvider::LockForWrite(viz::ResourceId id) {
  DCHECK(CanLockForWrite(id));
  viz::internal::Resource* resource = GetResource(id);
  WaitSyncTokenInternal(resource);
  resource->SetLocallyUsed();
  resource->locked_for_write = true;
  resource->mipmap_state = viz::internal::Resource::INVALID;
  return resource;
}

bool ResourceProvider::CanLockForWrite(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return !resource->locked_for_write && !resource->lock_for_read_count &&
         !resource->exported_count &&
         resource->origin == viz::internal::Resource::INTERNAL &&
         !resource->lost && ReadLockFenceHasPassed(resource);
}

bool ResourceProvider::IsOverlayCandidate(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->is_overlay_candidate;
}

gfx::BufferFormat ResourceProvider::GetBufferFormat(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->buffer_format;
}

#if defined(OS_ANDROID)
bool ResourceProvider::IsBackedBySurfaceTexture(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->is_backed_by_surface_texture;
}

bool ResourceProvider::WantsPromotionHintForTesting(viz::ResourceId id) {
  return wants_promotion_hints_set_.count(id) > 0;
}

size_t ResourceProvider::CountPromotionHintRequestsForTesting() {
  return wants_promotion_hints_set_.size();
}
#endif

void ResourceProvider::UnlockForWrite(viz::internal::Resource* resource) {
  DCHECK(resource->locked_for_write);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(resource->origin == viz::internal::Resource::INTERNAL);
  resource->locked_for_write = false;
}

void ResourceProvider::EnableReadLockFencesForTesting(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  DCHECK(resource);
  resource->read_lock_fences_enabled = true;
}

ResourceProvider::ScopedWriteLockGL::ScopedWriteLockGL(
    ResourceProvider* resource_provider,
    viz::ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  viz::internal::Resource* resource =
      resource_provider->LockForWrite(resource_id);
  DCHECK_EQ(resource->type, viz::ResourceType::kTexture);
  resource_provider->CreateTexture(resource);
  size_ = resource->size;
  format_ = resource->format;
  color_space_ = resource_provider_->GetResourceColorSpaceForRaster(resource);
  texture_id_ = resource->gl_id;
  target_ = resource->target;
  hint_ = resource->hint;
  mailbox_ = resource->mailbox;
  is_overlay_ = resource->is_overlay_candidate;
  allocated_ = resource->allocated;
}

ResourceProvider::ScopedWriteLockGL::~ScopedWriteLockGL() {
  viz::internal::Resource* resource =
      resource_provider_->GetResource(resource_id_);
  DCHECK(resource->locked_for_write);
  resource->allocated = allocated_;
  resource->mailbox = mailbox_;
  // Don't set empty sync token otherwise resource will be marked synchronized.
  if (has_sync_token_)
    resource->UpdateSyncToken(sync_token_);
  if (synchronized_)
    resource->SetSynchronized();
  if (generate_mipmap_)
    resource->SetGenerateMipmap();
  resource_provider_->UnlockForWrite(resource);
}

GrPixelConfig ResourceProvider::ScopedWriteLockGL::PixelConfig() const {
  return ToGrPixelConfig(format_);
}

GLuint ResourceProvider::ScopedWriteLockGL::GetTexture() {
  LazyAllocate(resource_provider_->ContextGL(), texture_id_);
  return texture_id_;
}

void ResourceProvider::ScopedWriteLockGL::CreateMailbox() {
  if (!mailbox_.IsZero())
    return;
  gpu::gles2::GLES2Interface* gl = resource_provider_->ContextGL();
  DCHECK(gl);
  gl->GenMailboxCHROMIUM(mailbox_.name);
  gl->ProduceTextureDirectCHROMIUM(texture_id_, target_, mailbox_.name);
}

GLuint ResourceProvider::ScopedWriteLockGL::ConsumeTexture(
    gpu::gles2::GLES2Interface* gl) {
  DCHECK(gl);
  DCHECK(!mailbox_.IsZero());

  GLuint texture_id =
      gl->CreateAndConsumeTextureCHROMIUM(target_, mailbox_.name);
  DCHECK(texture_id);

  LazyAllocate(gl, texture_id);

  return texture_id;
}

void ResourceProvider::ScopedWriteLockGL::LazyAllocate(
    gpu::gles2::GLES2Interface* gl,
    GLuint texture_id) {
  // ETC1 resources cannot be preallocated.
  if (format_ == viz::ETC1)
    return;

  if (allocated_)
    return;
  allocated_ = true;

  const ResourceProvider::Settings& settings = resource_provider_->settings_;

  gl->BindTexture(target_, texture_id);

  if (is_overlay_) {
    DCHECK(settings.use_texture_storage_image);
    gl->TexStorage2DImageCHROMIUM(target_, viz::TextureStorageFormat(format_),
                                  GL_SCANOUT_CHROMIUM, size_.width(),
                                  size_.height());
    if (color_space_.IsValid()) {
      gl->SetColorSpaceMetadataCHROMIUM(
          texture_id, reinterpret_cast<GLColorSpace>(&color_space_));
    }
  } else if (settings.use_texture_storage) {
    GLint levels = 1;
    if (settings.use_texture_npot &&
        (hint_ & viz::ResourceTextureHint::kMipmap)) {
      levels += base::bits::Log2Floor(std::max(size_.width(), size_.height()));
    }
    gl->TexStorage2DEXT(target_, levels, viz::TextureStorageFormat(format_),
                        size_.width(), size_.height());
  } else {
    gl->TexImage2D(target_, 0, GLInternalFormat(format_), size_.width(),
                   size_.height(), 0, GLDataFormat(format_),
                   GLDataType(format_), nullptr);
  }
}

ResourceProvider::ScopedSkSurface::ScopedSkSurface(GrContext* gr_context,
                                                   GLuint texture_id,
                                                   GLenum texture_target,
                                                   const gfx::Size& size,
                                                   viz::ResourceFormat format,
                                                   bool use_distance_field_text,
                                                   bool can_use_lcd_text,
                                                   int msaa_sample_count) {
  GrGLTextureInfo texture_info;
  texture_info.fID = texture_id;
  texture_info.fTarget = texture_target;
  GrBackendTexture backend_texture(size.width(), size.height(),
                                   ToGrPixelConfig(format), texture_info);
  uint32_t flags =
      use_distance_field_text ? SkSurfaceProps::kUseDistanceFieldFonts_Flag : 0;
  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
  if (can_use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props =
        SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  }
  surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
      gr_context, backend_texture, kTopLeft_GrSurfaceOrigin, msaa_sample_count,
      nullptr, &surface_props);
}

ResourceProvider::ScopedSkSurface::~ScopedSkSurface() {
  if (surface_)
    surface_->prepareForExternalIO();
}

void ResourceProvider::PopulateSkBitmapWithResource(
    SkBitmap* sk_bitmap,
    const viz::internal::Resource* resource) {
  DCHECK_EQ(viz::RGBA_8888, resource->format);
  SkImageInfo info = SkImageInfo::MakeN32Premul(resource->size.width(),
                                                resource->size.height());
  sk_bitmap->installPixels(info, resource->pixels, info.minRowBytes());
}

ResourceProvider::ScopedWriteLockSoftware::ScopedWriteLockSoftware(
    ResourceProvider* resource_provider,
    viz::ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  viz::internal::Resource* resource =
      resource_provider->LockForWrite(resource_id);
  resource_provider->PopulateSkBitmapWithResource(&sk_bitmap_, resource);
  color_space_ = resource_provider->GetResourceColorSpaceForRaster(resource);
  DCHECK(valid());
}

ResourceProvider::ScopedWriteLockSoftware::~ScopedWriteLockSoftware() {
  viz::internal::Resource* resource =
      resource_provider_->GetResource(resource_id_);
  resource->SetSynchronized();
  resource_provider_->UnlockForWrite(resource);
}

ResourceProvider::SynchronousFence::SynchronousFence(
    gpu::gles2::GLES2Interface* gl)
    : gl_(gl), has_synchronized_(true) {}

ResourceProvider::SynchronousFence::~SynchronousFence() {}

void ResourceProvider::SynchronousFence::Set() {
  has_synchronized_ = false;
}

bool ResourceProvider::SynchronousFence::HasPassed() {
  if (!has_synchronized_) {
    has_synchronized_ = true;
    Synchronize();
  }
  return true;
}

void ResourceProvider::SynchronousFence::Wait() {
  HasPassed();
}

void ResourceProvider::SynchronousFence::Synchronize() {
  TRACE_EVENT0("cc", "ResourceProvider::SynchronousFence::Synchronize");
  gl_->Finish();
}

GLenum ResourceProvider::BindForSampling(viz::ResourceId resource_id,
                                         GLenum unit,
                                         GLenum filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GLES2Interface* gl = ContextGL();
  ResourceMap::iterator it = resources_.find(resource_id);
  DCHECK(it != resources_.end());
  viz::internal::Resource* resource = &it->second;
  DCHECK(resource->lock_for_read_count);
  DCHECK(!resource->locked_for_write);

  ScopedSetActiveTexture scoped_active_tex(gl, unit);
  GLenum target = resource->target;
  gl->BindTexture(target, resource->gl_id);
  GLenum min_filter = filter;
  if (filter == GL_LINEAR) {
    switch (resource->mipmap_state) {
      case viz::internal::Resource::INVALID:
        break;
      case viz::internal::Resource::GENERATE:
        DCHECK(settings_.use_texture_npot);
        gl->GenerateMipmap(target);
        resource->mipmap_state = viz::internal::Resource::VALID;
      // fall-through
      case viz::internal::Resource::VALID:
        min_filter = GL_LINEAR_MIPMAP_LINEAR;
        break;
    }
  }
  if (min_filter != resource->min_filter) {
    gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
    resource->min_filter = min_filter;
  }
  if (filter != resource->filter) {
    gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    resource->filter = filter;
  }

  return target;
}

void ResourceProvider::CreateForTesting(viz::ResourceId id) {
  CreateTexture(GetResource(id));
}

void ResourceProvider::CreateTexture(viz::internal::Resource* resource) {
  if (!resource->is_gpu_resource_type())
    return;

  if (resource->gl_id)
    return;

  DCHECK_EQ(resource->origin, viz::internal::Resource::INTERNAL);
  DCHECK(resource->mailbox.IsZero());

  resource->gl_id = texture_id_allocator_->NextId();
  DCHECK(resource->gl_id);

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);

  // Create and set texture properties. Allocation is delayed until needed.
  gl->BindTexture(resource->target, resource->gl_id);
  gl->TexParameteri(resource->target, GL_TEXTURE_MIN_FILTER,
                    resource->original_filter);
  gl->TexParameteri(resource->target, GL_TEXTURE_MAG_FILTER,
                    resource->original_filter);
  gl->TexParameteri(resource->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(resource->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (settings_.use_texture_usage_hint &&
      (resource->hint & viz::ResourceTextureHint::kFramebuffer)) {
    gl->TexParameteri(resource->target, GL_TEXTURE_USAGE_ANGLE,
                      GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  }
}

void ResourceProvider::CreateMailbox(viz::internal::Resource* resource) {
  if (!resource->is_gpu_resource_type())
    return;

  if (!resource->mailbox.IsZero())
    return;

  CreateTexture(resource);

  DCHECK(resource->gl_id);
  DCHECK_EQ(resource->origin, viz::internal::Resource::INTERNAL);

  gpu::gles2::GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  gl->GenMailboxCHROMIUM(resource->mailbox.name);
  gl->ProduceTextureDirectCHROMIUM(resource->gl_id, resource->target,
                                   resource->mailbox.name);
  resource->SetLocallyUsed();
}

void ResourceProvider::AllocateForTesting(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  if (!resource->allocated) {
    // Software and external resources are marked allocated on creation.
    DCHECK(resource->is_gpu_resource_type());
    DCHECK_EQ(resource->origin, viz::internal::Resource::INTERNAL);
    ScopedWriteLockGL resource_lock(this, id);
    resource_lock.GetTexture();  // Allocates texture.
  }
}

void ResourceProvider::CreateAndBindImage(viz::internal::Resource* resource) {
  DCHECK(resource->gpu_memory_buffer);
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
  // TODO(reveman): This avoids a performance problem on ARM ChromeOS
  // devices. This only works with shared memory backed buffers.
  // crbug.com/580166
  DCHECK_EQ(resource->gpu_memory_buffer->GetHandle().type,
            gfx::SHARED_MEMORY_BUFFER);
#endif
  CreateTexture(resource);

  gpu::gles2::GLES2Interface* gl = ContextGL();
  DCHECK(gl);

  gl->BindTexture(resource->target, resource->gl_id);

  if (!resource->image_id) {
    resource->image_id = gl->CreateImageCHROMIUM(
        resource->gpu_memory_buffer->AsClientBuffer(), resource->size.width(),
        resource->size.height(), GLInternalFormat(resource->format));

    DCHECK(resource->image_id ||
           gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);

    gl->BindTexImage2DCHROMIUM(resource->target, resource->image_id);
  } else {
    gl->ReleaseTexImage2DCHROMIUM(resource->target, resource->image_id);
    gl->BindTexImage2DCHROMIUM(resource->target, resource->image_id);
  }
}

void ResourceProvider::WaitSyncToken(viz::ResourceId id) {
  WaitSyncTokenInternal(GetResource(id));
}

void ResourceProvider::WaitSyncTokenInternal(
    viz::internal::Resource* resource) {
  DCHECK(resource);
  if (!resource->ShouldWaitSyncToken())
    return;
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  // In the case of context lost, this sync token may be empty (see comment in
  // the UpdateSyncToken() function). The WaitSyncTokenCHROMIUM() function
  // handles empty sync tokens properly so just wait anyways and update the
  // state the synchronized.
  gl->WaitSyncTokenCHROMIUM(resource->sync_token().GetConstData());
  resource->SetSynchronized();
}

GLint ResourceProvider::GetActiveTextureUnit(gpu::gles2::GLES2Interface* gl) {
  GLint active_unit = 0;
  gl->GetIntegerv(GL_ACTIVE_TEXTURE, &active_unit);
  return active_unit;
}

gpu::SyncToken ResourceProvider::GenerateSyncTokenHelper(
    gpu::gles2::GLES2Interface* gl) {
  DCHECK(gl);
  const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();

  // Barrier to sync worker context output to cc context.
  gl->OrderingBarrierCHROMIUM();

  // Generate sync token after the barrier for cross context synchronization.
  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  DCHECK(sync_token.HasData() ||
         gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);

  return sync_token;
}

GLenum ResourceProvider::GetImageTextureTarget(
    gfx::BufferUsage usage,
    viz::ResourceFormat format) const {
  gfx::BufferFormat buffer_format = BufferFormat(format);
  auto found = buffer_to_texture_target_map_.find(
      viz::BufferToTextureTargetKey(usage, buffer_format));
  DCHECK(found != buffer_to_texture_target_map_.end());
  return found->second;
}

GLES2Interface* ResourceProvider::ContextGL() const {
  viz::ContextProvider* context_provider = compositor_context_provider_;
  return context_provider ? context_provider->ContextGL() : nullptr;
}

bool ResourceProvider::IsGLContextLost() const {
  return ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

bool ResourceProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const uint64_t tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();

  for (const auto& resource_entry : resources_) {
    const auto& resource = resource_entry.second;

    bool backing_memory_allocated = false;
    switch (resource.type) {
      case viz::ResourceType::kGpuMemoryBuffer:
        backing_memory_allocated = !!resource.gpu_memory_buffer;
        break;
      case viz::ResourceType::kTexture:
        backing_memory_allocated = !!resource.gl_id;
        break;
      case viz::ResourceType::kBitmap:
        backing_memory_allocated = resource.has_shared_bitmap_id;
        break;
    }

    if (!backing_memory_allocated) {
      // Don't log unallocated resources - they have no backing memory.
      continue;
    }

    // ResourceIds are not process-unique, so log with the ResourceProvider's
    // unique id.
    std::string dump_name =
        base::StringPrintf("cc/resource_memory/provider_%d/resource_%d",
                           tracing_id_, resource_entry.first);
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);

    uint64_t total_bytes = ResourceUtil::UncheckedSizeInBytesAligned<size_t>(
        resource.size, resource.format);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    static_cast<uint64_t>(total_bytes));

    // Resources may be shared across processes and require a shared GUID to
    // prevent double counting the memory.
    base::trace_event::MemoryAllocatorDumpGuid guid;
    base::UnguessableToken shared_memory_guid;
    switch (resource.type) {
      case viz::ResourceType::kGpuMemoryBuffer:
        guid =
            resource.gpu_memory_buffer->GetGUIDForTracing(tracing_process_id);
        shared_memory_guid =
            resource.gpu_memory_buffer->GetHandle().handle.GetGUID();
        break;
      case viz::ResourceType::kTexture:
        DCHECK(resource.gl_id);
        guid = gl::GetGLTextureClientGUIDForTracing(
            compositor_context_provider_->ContextSupport()
                ->ShareGroupTracingGUID(),
            resource.gl_id);
        break;
      case viz::ResourceType::kBitmap:
        DCHECK(resource.has_shared_bitmap_id);
        guid = viz::GetSharedBitmapGUIDForTracing(resource.shared_bitmap_id);
        if (resource.shared_bitmap) {
          shared_memory_guid =
              resource.shared_bitmap->GetSharedMemoryHandle().GetGUID();
        }
        break;
    }

    DCHECK(!guid.empty());

    const int kImportance = 2;
    if (!shared_memory_guid.is_empty()) {
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                           kImportance);
    } else {
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  return true;
}

}  // namespace cc

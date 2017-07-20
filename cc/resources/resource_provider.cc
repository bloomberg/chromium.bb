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
#include "cc/resources/resource_util.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/transferable_resource.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
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

bool IsGpuResourceType(ResourceProvider::ResourceType type) {
  return type != ResourceProvider::RESOURCE_TYPE_BITMAP;
}

GLenum TextureToStorageFormat(viz::ResourceFormat format) {
  GLenum storage_format = GL_RGBA8_OES;
  switch (format) {
    case viz::RGBA_8888:
      break;
    case viz::BGRA_8888:
      storage_format = GL_BGRA8_EXT;
      break;
    case viz::RGBA_F16:
      storage_format = GL_RGBA16F_EXT;
      break;
    case viz::RGBA_4444:
    case viz::ALPHA_8:
    case viz::LUMINANCE_8:
    case viz::RGB_565:
    case viz::ETC1:
    case viz::RED_8:
    case viz::LUMINANCE_F16:
      NOTREACHED();
      break;
  }

  return storage_format;
}

bool IsFormatSupportedForStorage(viz::ResourceFormat format, bool use_bgra) {
  switch (format) {
    case viz::RGBA_8888:
    case viz::RGBA_F16:
      return true;
    case viz::BGRA_8888:
      return use_bgra;
    case viz::RGBA_4444:
    case viz::ALPHA_8:
    case viz::LUMINANCE_8:
    case viz::RGB_565:
    case viz::ETC1:
    case viz::RED_8:
    case viz::LUMINANCE_F16:
      return false;
  }
  return false;
}

GrPixelConfig ToGrPixelConfig(viz::ResourceFormat format) {
  switch (format) {
    case viz::RGBA_8888:
      return kRGBA_8888_GrPixelConfig;
    case viz::BGRA_8888:
      return kBGRA_8888_GrPixelConfig;
    case viz::RGBA_4444:
      return kRGBA_4444_GrPixelConfig;
    case viz::RGBA_F16:
      return kRGBA_half_GrPixelConfig;
    default:
      break;
  }
  DCHECK(false) << "Unsupported resource format.";
  return kSkia8888_GrPixelConfig;
}

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

ResourceProvider::Resource::~Resource() {}

ResourceProvider::Resource::Resource(GLuint texture_id,
                                     const gfx::Size& size,
                                     Origin origin,
                                     GLenum target,
                                     GLenum filter,
                                     TextureHint hint,
                                     ResourceType type,
                                     viz::ResourceFormat format)
    : child_id(0),
      gl_id(texture_id),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      gl_read_lock_query_id(0),
      pixels(nullptr),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      dirty_image(false),
      locked_for_write(false),
      lost(false),
      marked_for_deletion(false),
      allocated(false),
      read_lock_fences_enabled(false),
      has_shared_bitmap_id(false),
      is_overlay_candidate(false),
#if defined(OS_ANDROID)
      is_backed_by_surface_texture(false),
      wants_promotion_hint(false),
#endif
      read_lock_fence(nullptr),
      size(size),
      origin(origin),
      target(target),
      original_filter(filter),
      filter(filter),
      image_id(0),
      bound_image_id(0),
      hint(hint),
      type(type),
      usage(gfx::BufferUsage::GPU_READ_CPU_READ_WRITE),
      buffer_format(gfx::BufferFormat::RGBA_8888),
      format(format),
      shared_bitmap(nullptr) {
}

ResourceProvider::Resource::Resource(uint8_t* pixels,
                                     viz::SharedBitmap* bitmap,
                                     const gfx::Size& size,
                                     Origin origin,
                                     GLenum filter)
    : child_id(0),
      gl_id(0),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      gl_read_lock_query_id(0),
      pixels(pixels),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      dirty_image(false),
      locked_for_write(false),
      lost(false),
      marked_for_deletion(false),
      allocated(false),
      read_lock_fences_enabled(false),
      has_shared_bitmap_id(!!bitmap),
      is_overlay_candidate(false),
#if defined(OS_ANDROID)
      is_backed_by_surface_texture(false),
      wants_promotion_hint(false),
#endif
      read_lock_fence(nullptr),
      size(size),
      origin(origin),
      target(0),
      original_filter(filter),
      filter(filter),
      image_id(0),
      bound_image_id(0),
      hint(TEXTURE_HINT_IMMUTABLE),
      type(RESOURCE_TYPE_BITMAP),
      buffer_format(gfx::BufferFormat::RGBA_8888),
      format(viz::RGBA_8888),
      shared_bitmap(bitmap) {
  DCHECK(origin == DELEGATED || pixels);
  if (bitmap)
    shared_bitmap_id = bitmap->id();
}

ResourceProvider::Resource::Resource(const viz::SharedBitmapId& bitmap_id,
                                     const gfx::Size& size,
                                     Origin origin,
                                     GLenum filter)
    : child_id(0),
      gl_id(0),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      gl_read_lock_query_id(0),
      pixels(nullptr),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      dirty_image(false),
      locked_for_write(false),
      lost(false),
      marked_for_deletion(false),
      allocated(false),
      read_lock_fences_enabled(false),
      has_shared_bitmap_id(true),
      is_overlay_candidate(false),
#if defined(OS_ANDROID)
      is_backed_by_surface_texture(false),
      wants_promotion_hint(false),
#endif
      read_lock_fence(nullptr),
      size(size),
      origin(origin),
      target(0),
      original_filter(filter),
      filter(filter),
      image_id(0),
      bound_image_id(0),
      hint(TEXTURE_HINT_IMMUTABLE),
      type(RESOURCE_TYPE_BITMAP),
      buffer_format(gfx::BufferFormat::RGBA_8888),
      format(viz::RGBA_8888),
      shared_bitmap_id(bitmap_id),
      shared_bitmap(nullptr) {
}

ResourceProvider::Resource::Resource(Resource&& other) = default;

void ResourceProvider::Resource::set_mailbox(
    const viz::TextureMailbox& mailbox) {
  mailbox_ = mailbox;
  if (IsGpuResourceType(type)) {
    synchronization_state_ =
        (mailbox.sync_token().HasData() ? NEEDS_WAIT : LOCALLY_USED);
    needs_sync_token_ = !mailbox.sync_token().HasData();
  } else {
    synchronization_state_ = SYNCHRONIZED;
  }
}

void ResourceProvider::Resource::SetLocallyUsed() {
  synchronization_state_ = LOCALLY_USED;
  mailbox_.set_sync_token(gpu::SyncToken());
  needs_sync_token_ = IsGpuResourceType(type);
}

void ResourceProvider::Resource::SetSynchronized() {
  synchronization_state_ = SYNCHRONIZED;
}

void ResourceProvider::Resource::UpdateSyncToken(
    const gpu::SyncToken& sync_token) {
  // In the case of context lost, this sync token may be empty since sync tokens
  // may not be generated unless a successful flush occurred. However, we will
  // assume the task runner is calling this function properly and update the
  // state accordingly.
  mailbox_.set_sync_token(sync_token);
  synchronization_state_ = NEEDS_WAIT;
  needs_sync_token_ = false;
}

int8_t* ResourceProvider::Resource::GetSyncTokenData() {
  return mailbox_.GetSyncTokenData();
}

void ResourceProvider::Resource::WaitSyncToken(gpu::gles2::GLES2Interface* gl) {
  // Make sure we are only called when state actually needs to wait.
  DCHECK_EQ(NEEDS_WAIT, synchronization_state_);

  // Make sure sync token is not stale.
  DCHECK(!needs_sync_token_);

  // In the case of context lost, this sync token may be empty (see comment in
  // the UpdateSyncToken() function). The WaitSyncTokenCHROMIUM() function
  // handles empty sync tokens properly so just wait anyways and update the
  // state the synchronized.
  gl->WaitSyncTokenCHROMIUM(mailbox_.sync_token().GetConstData());
  SetSynchronized();
}

ResourceProvider::Child::Child()
    : marked_for_deletion(false), needs_sync_tokens(true) {}

ResourceProvider::Child::Child(const Child& other) = default;

ResourceProvider::Child::~Child() {}

ResourceProvider::Settings::Settings(
    viz::ContextProvider* compositor_context_provider,
    bool delegated_sync_points_required,
    bool enable_color_correct_rasterization,
    const viz::ResourceSettings& resource_settings)
    : default_resource_type(resource_settings.use_gpu_memory_buffer_resources
                                ? RESOURCE_TYPE_GPU_MEMORY_BUFFER
                                : RESOURCE_TYPE_GL_TEXTURE),
      enable_color_correct_rasterization(enable_color_correct_rasterization),
      delegated_sync_points_required(delegated_sync_points_required) {
  if (!compositor_context_provider) {
    default_resource_type = RESOURCE_TYPE_BITMAP;
    // Pick an arbitrary limit here similar to what hardware might.
    max_texture_size = 16 * 1024;
    best_texture_format = viz::RGBA_8888;
    return;
  }

  DCHECK(IsGpuResourceType(default_resource_type));

  const auto& caps = compositor_context_provider->ContextCapabilities();
  use_texture_storage_ext = caps.texture_storage;
  use_texture_format_bgra = caps.texture_format_bgra8888;
  use_texture_usage_hint = caps.texture_usage;
  use_sync_query = caps.sync_query;

  if (caps.disable_one_component_textures) {
    yuv_resource_format = yuv_highbit_resource_format = viz::RGBA_8888;
  } else {
    yuv_resource_format = caps.texture_rg ? viz::RED_8 : viz::LUMINANCE_8;
    yuv_highbit_resource_format = caps.texture_half_float_linear
                                      ? viz::LUMINANCE_F16
                                      : yuv_resource_format;
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
    BlockingTaskRunner* blocking_main_thread_task_runner,
    bool delegated_sync_points_required,
    bool enable_color_correct_rasterization,
    const viz::ResourceSettings& resource_settings)
    : settings_(compositor_context_provider,
                delegated_sync_points_required,
                enable_color_correct_rasterization,
                resource_settings),
      compositor_context_provider_(compositor_context_provider),
      shared_bitmap_manager_(shared_bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      blocking_main_thread_task_runner_(blocking_main_thread_task_runner),
      lost_context_provider_(false),
      next_id_(1),
      next_child_(1),
      buffer_to_texture_target_map_(
          resource_settings.buffer_to_texture_target_map),
      tracing_id_(g_next_resource_provider_tracing_id.GetNext()) {
  DCHECK(resource_settings.texture_id_allocation_chunk_size);
  DCHECK(thread_checker_.CalledOnValidThread());

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

  while (!children_.empty())
    DestroyChildInternal(children_.begin(), FOR_SHUTDOWN);
  while (!resources_.empty())
    DeleteResourceInternal(resources_.begin(), FOR_SHUTDOWN);

  GLES2Interface* gl = ContextGL();
  if (!IsGpuResourceType(settings_.default_resource_type)) {
    // We are not in GL mode, but double check before returning.
    DCHECK(!gl);
    return;
  }

  DCHECK(gl);
#if DCHECK_IS_ON()
  // Check that all GL resources has been deleted.
  for (ResourceMap::const_iterator itr = resources_.begin();
       itr != resources_.end(); ++itr) {
    DCHECK(!IsGpuResourceType(itr->second.type));
  }
#endif  // DCHECK_IS_ON()

  texture_id_allocator_ = nullptr;
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
      // We don't currently render into these formats. If we need to render into
      // these eventually, we should expand this logic.
      return false;
  }

  NOTREACHED();
  return false;
}

bool ResourceProvider::InUseByConsumer(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->lock_for_read_count > 0 || resource->exported_count > 0 ||
         resource->lost;
}

bool ResourceProvider::IsLost(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->lost;
}

void ResourceProvider::LoseResourceForTesting(ResourceId id) {
  Resource* resource = GetResource(id);
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

ResourceId ResourceProvider::CreateResource(
    const gfx::Size& size,
    TextureHint hint,
    viz::ResourceFormat format,
    const gfx::ColorSpace& color_space) {
  DCHECK(!size.IsEmpty());
  switch (settings_.default_resource_type) {
    case RESOURCE_TYPE_GPU_MEMORY_BUFFER:
      // GPU memory buffers don't support viz::LUMINANCE_F16 yet.
      if (format != viz::LUMINANCE_F16) {
        return CreateGLTexture(
            size, hint, RESOURCE_TYPE_GPU_MEMORY_BUFFER, format,
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE, color_space);
      }
    // Fall through and use a regular texture.
    case RESOURCE_TYPE_GL_TEXTURE:
      return CreateGLTexture(size, hint, RESOURCE_TYPE_GL_TEXTURE, format,
                             gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                             color_space);

    case RESOURCE_TYPE_BITMAP:
      DCHECK_EQ(viz::RGBA_8888, format);
      return CreateBitmap(size, color_space);
  }

  LOG(FATAL) << "Invalid default resource type.";
  return 0;
}

ResourceId ResourceProvider::CreateGpuMemoryBufferResource(
    const gfx::Size& size,
    TextureHint hint,
    viz::ResourceFormat format,
    gfx::BufferUsage usage,
    const gfx::ColorSpace& color_space) {
  DCHECK(!size.IsEmpty());
  switch (settings_.default_resource_type) {
    case RESOURCE_TYPE_GPU_MEMORY_BUFFER:
    case RESOURCE_TYPE_GL_TEXTURE: {
      return CreateGLTexture(size, hint, RESOURCE_TYPE_GPU_MEMORY_BUFFER,
                             format, usage, color_space);
    }
    case RESOURCE_TYPE_BITMAP:
      DCHECK_EQ(viz::RGBA_8888, format);
      return CreateBitmap(size, color_space);
  }

  LOG(FATAL) << "Invalid default resource type.";
  return 0;
}

ResourceId ResourceProvider::CreateGLTexture(
    const gfx::Size& size,
    TextureHint hint,
    ResourceType type,
    viz::ResourceFormat format,
    gfx::BufferUsage usage,
    const gfx::ColorSpace& color_space) {
  DCHECK_LE(size.width(), settings_.max_texture_size);
  DCHECK_LE(size.height(), settings_.max_texture_size);
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(crbug.com/590317): We should not assume that all resources created by
  // ResourceProvider are GPU_READ_CPU_READ_WRITE. We should determine this
  // based on the current RasterBufferProvider's needs.
  GLenum target = type == RESOURCE_TYPE_GPU_MEMORY_BUFFER
                      ? GetImageTextureTarget(usage, format)
                      : GL_TEXTURE_2D;

  ResourceId id = next_id_++;
  Resource* resource =
      InsertResource(id, Resource(0, size, Resource::INTERNAL, target,
                                  GL_LINEAR, hint, type, format));
  resource->usage = usage;
  resource->allocated = false;
  resource->color_space = color_space;
  return id;
}

ResourceId ResourceProvider::CreateBitmap(const gfx::Size& size,
                                          const gfx::ColorSpace& color_space) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<viz::SharedBitmap> bitmap =
      shared_bitmap_manager_->AllocateSharedBitmap(size);
  uint8_t* pixels = bitmap->pixels();
  DCHECK(pixels);

  ResourceId id = next_id_++;
  Resource* resource = InsertResource(
      id,
      Resource(pixels, bitmap.release(), size, Resource::INTERNAL, GL_LINEAR));
  resource->allocated = true;
  resource->color_space = color_space;
  return id;
}

ResourceId ResourceProvider::CreateResourceFromTextureMailbox(
    const viz::TextureMailbox& mailbox,
    std::unique_ptr<SingleReleaseCallbackImpl> release_callback_impl,
    bool read_lock_fences_enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Just store the information. Mailbox will be consumed in LockForRead().
  ResourceId id = next_id_++;
  DCHECK(mailbox.IsValid());
  Resource* resource = nullptr;
  if (mailbox.IsTexture()) {
    resource = InsertResource(
        id, Resource(0, mailbox.size_in_pixels(), Resource::EXTERNAL,
                     mailbox.target(),
                     mailbox.nearest_neighbor() ? GL_NEAREST : GL_LINEAR,
                     TEXTURE_HINT_IMMUTABLE, RESOURCE_TYPE_GL_TEXTURE,
                     viz::RGBA_8888));
  } else {
    DCHECK(mailbox.IsSharedMemory());
    viz::SharedBitmap* shared_bitmap = mailbox.shared_bitmap();
    uint8_t* pixels = shared_bitmap->pixels();
    DCHECK(pixels);
    resource = InsertResource(
        id, Resource(pixels, shared_bitmap, mailbox.size_in_pixels(),
                     Resource::EXTERNAL, GL_LINEAR));
  }
  resource->allocated = true;
  resource->set_mailbox(mailbox);
  resource->color_space = mailbox.color_space();
  resource->release_callback_impl =
      base::Bind(&SingleReleaseCallbackImpl::Run,
                 base::Owned(release_callback_impl.release()));
  resource->read_lock_fences_enabled = read_lock_fences_enabled;
  resource->is_overlay_candidate = mailbox.is_overlay_candidate();
#if defined(OS_ANDROID)
  resource->is_backed_by_surface_texture =
      mailbox.is_backed_by_surface_texture();
  resource->wants_promotion_hint = mailbox.wants_promotion_hint();
  if (resource->wants_promotion_hint)
    wants_promotion_hints_set_.insert(id);
#endif
  resource->color_space = mailbox.color_space();

  return id;
}

ResourceId ResourceProvider::CreateResourceFromTextureMailbox(
    const viz::TextureMailbox& mailbox,
    std::unique_ptr<SingleReleaseCallbackImpl> release_callback_impl) {
  return CreateResourceFromTextureMailbox(
      mailbox, std::move(release_callback_impl), false);
}

void ResourceProvider::DeleteResource(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
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
  Resource* resource = &it->second;
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
      IsGpuResourceType(resource->type) && lost_context_provider_;
  bool lost_resource =
      resource->lost || exported_resource_lost || gpu_resource_lost;

  // Wait on sync token before deleting resources we own.
  if (!lost_resource && resource->origin == Resource::INTERNAL &&
      resource->synchronization_state() == Resource::NEEDS_WAIT) {
    DCHECK(resource->allocated);
    DCHECK(IsGpuResourceType(resource->type));
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    resource->WaitSyncToken(gl);
  }

  if (resource->image_id) {
    DCHECK(resource->origin == Resource::INTERNAL);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DestroyImageCHROMIUM(resource->image_id);
  }
  if (resource->gl_upload_query_id) {
    DCHECK(resource->origin == Resource::INTERNAL);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DeleteQueriesEXT(1, &resource->gl_upload_query_id);
  }
  if (resource->gl_read_lock_query_id) {
    DCHECK(resource->origin == Resource::INTERNAL);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DeleteQueriesEXT(1, &resource->gl_read_lock_query_id);
  }
  if (resource->gl_pixel_buffer_id) {
    DCHECK(resource->origin == Resource::INTERNAL);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DeleteBuffers(1, &resource->gl_pixel_buffer_id);
  }
  if (resource->origin == Resource::EXTERNAL) {
    DCHECK(resource->mailbox().IsValid());
    gpu::SyncToken sync_token = resource->mailbox().sync_token();
    if (IsGpuResourceType(resource->type)) {
      DCHECK(resource->mailbox().IsTexture());
      GLES2Interface* gl = ContextGL();
      DCHECK(gl);
      if (resource->gl_id) {
        DCHECK_NE(Resource::NEEDS_WAIT, resource->synchronization_state());
        gl->DeleteTextures(1, &resource->gl_id);
        resource->gl_id = 0;
        if (!lost_resource) {
          const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
          gl->ShallowFlushCHROMIUM();
          gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
        }
      }
    } else {
      DCHECK(resource->mailbox().IsSharedMemory());
      resource->shared_bitmap = nullptr;
      resource->pixels = nullptr;
    }
    resource->release_callback_impl.Run(sync_token, lost_resource,
                                        blocking_main_thread_task_runner_);
  }
  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DeleteTextures(1, &resource->gl_id);
    resource->gl_id = 0;
  }
  if (resource->shared_bitmap) {
    DCHECK(resource->origin != Resource::EXTERNAL);
    DCHECK_EQ(RESOURCE_TYPE_BITMAP, resource->type);
    delete resource->shared_bitmap;
    resource->pixels = nullptr;
  }
  if (resource->pixels) {
    DCHECK(resource->origin == Resource::INTERNAL);
    delete[] resource->pixels;
    resource->pixels = nullptr;
  }
  if (resource->gpu_memory_buffer) {
    DCHECK(resource->origin == Resource::INTERNAL ||
           resource->origin == Resource::DELEGATED);
    resource->gpu_memory_buffer.reset();
  }
  resources_.erase(it);
}

void ResourceProvider::FlushPendingDeletions() const {
  if (auto* gl = ContextGL())
    gl->ShallowFlushCHROMIUM();
}

ResourceProvider::ResourceType ResourceProvider::GetResourceType(
    ResourceId id) {
  return GetResource(id)->type;
}

GLenum ResourceProvider::GetResourceTextureTarget(ResourceId id) {
  return GetResource(id)->target;
}

bool ResourceProvider::IsImmutable(ResourceId id) {
  if (IsGpuResourceType(settings_.default_resource_type)) {
    return GetTextureHint(id) == TEXTURE_HINT_IMMUTABLE;
  } else {
    // Software resources are immutable; they cannot change format or be
    // resized.
    return true;
  }
}

ResourceProvider::TextureHint ResourceProvider::GetTextureHint(ResourceId id) {
  return GetResource(id)->hint;
}

gfx::ColorSpace ResourceProvider::GetResourceColorSpaceForRaster(
    const Resource* resource) const {
  if (!settings_.enable_color_correct_rasterization)
    return gfx::ColorSpace();
  return resource->color_space;
}

void ResourceProvider::CopyToResource(ResourceId id,
                                      const uint8_t* image,
                                      const gfx::Size& image_size) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write);
  DCHECK(!resource->lock_for_read_count);
  DCHECK(resource->origin == Resource::INTERNAL);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(ReadLockFenceHasPassed(resource));

  DCHECK_EQ(image_size.width(), resource->size.width());
  DCHECK_EQ(image_size.height(), resource->size.height());

  if (resource->allocated)
    WaitSyncTokenIfNeeded(id);

  if (resource->type == RESOURCE_TYPE_BITMAP) {
    DCHECK_EQ(RESOURCE_TYPE_BITMAP, resource->type);
    DCHECK(resource->allocated);
    DCHECK_EQ(viz::RGBA_8888, resource->format);
    SkImageInfo source_info =
        SkImageInfo::MakeN32Premul(image_size.width(), image_size.height());
    size_t image_stride = image_size.width() * 4;

    ScopedWriteLockSoftware lock(this, id);
    SkCanvas dest(lock.sk_bitmap());
    dest.writePixels(source_info, image, image_stride, 0, 0);
  } else {
    ScopedWriteLockGL lock(this, id, false);
    unsigned resource_texture_id = lock.texture_id();
    DCHECK(resource_texture_id);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->BindTexture(resource->target, resource_texture_id);
    if (resource->format == viz::ETC1) {
      DCHECK_EQ(resource->target, static_cast<GLenum>(GL_TEXTURE_2D));
      int image_bytes =
          ResourceUtil::CheckedSizeInBytes<int>(image_size, viz::ETC1);
      gl->CompressedTexImage2D(resource->target, 0, GLInternalFormat(viz::ETC1),
                               image_size.width(), image_size.height(), 0,
                               image_bytes, image);
    } else {
      gl->TexSubImage2D(resource->target, 0, 0, 0, image_size.width(),
                        image_size.height(), GLDataFormat(resource->format),
                        GLDataType(resource->format), image);
    }
    const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->OrderingBarrierCHROMIUM();
    gpu::SyncToken sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
    lock.set_sync_token(sync_token);
    lock.set_synchronized(true);
  }
}

void ResourceProvider::GenerateSyncTokenForResource(ResourceId resource_id) {
  Resource* resource = GetResource(resource_id);
  if (!resource->needs_sync_token())
    return;

  gpu::SyncToken sync_token;
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);

  const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->OrderingBarrierCHROMIUM();
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  resource->UpdateSyncToken(sync_token);
  resource->SetSynchronized();
}

void ResourceProvider::GenerateSyncTokenForResources(
    const ResourceIdArray& resource_ids) {
  gpu::SyncToken sync_token;
  bool created_sync_token = false;
  for (ResourceId id : resource_ids) {
    Resource* resource = GetResource(id);
    if (resource->needs_sync_token()) {
      if (!created_sync_token) {
        GLES2Interface* gl = ContextGL();
        DCHECK(gl);

        const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();
        gl->OrderingBarrierCHROMIUM();
        gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
        created_sync_token = true;
      }

      resource->UpdateSyncToken(sync_token);
      resource->SetSynchronized();
    }
  }
}

gpu::SyncToken ResourceProvider::GetSyncTokenForResources(
    const ResourceIdArray& resource_ids) {
  gpu::SyncToken latest_sync_token;
  for (ResourceId id : resource_ids) {
    const gpu::SyncToken& sync_token = GetResource(id)->mailbox().sync_token();
    if (sync_token.release_count() > latest_sync_token.release_count())
      latest_sync_token = sync_token;
  }
  return latest_sync_token;
}

ResourceProvider::Resource* ResourceProvider::InsertResource(
    ResourceId id,
    Resource resource) {
  std::pair<ResourceMap::iterator, bool> result =
      resources_.insert(ResourceMap::value_type(id, std::move(resource)));
  DCHECK(result.second);
  return &result.first->second;
}

ResourceProvider::Resource* ResourceProvider::GetResource(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(id);
  ResourceMap::iterator it = resources_.find(id);
  DCHECK(it != resources_.end());
  return &it->second;
}

const ResourceProvider::Resource* ResourceProvider::LockForRead(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write) << "locked for write: "
                                      << resource->locked_for_write;
  DCHECK_EQ(resource->exported_count, 0);
  // Uninitialized! Call SetPixels or LockForWrite first.
  DCHECK(resource->allocated);

  // Mailbox sync_tokens must be processed by a call to
  // WaitSyncTokenIfNeeded() prior to calling LockForRead().
  DCHECK_NE(Resource::NEEDS_WAIT, resource->synchronization_state());

  LazyCreate(resource);

  if (IsGpuResourceType(resource->type) && !resource->gl_id) {
    DCHECK(resource->origin != Resource::INTERNAL);
    DCHECK(resource->mailbox().IsTexture());

    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    resource->gl_id = gl->CreateAndConsumeTextureCHROMIUM(
        resource->mailbox().target(), resource->mailbox().name());
    resource->SetLocallyUsed();
  }

  if (!resource->pixels && resource->has_shared_bitmap_id &&
      shared_bitmap_manager_) {
    std::unique_ptr<viz::SharedBitmap> bitmap =
        shared_bitmap_manager_->GetSharedBitmapFromId(
            resource->size, resource->shared_bitmap_id);
    if (bitmap) {
      resource->shared_bitmap = bitmap.release();
      resource->pixels = resource->shared_bitmap->pixels();
    }
  }

  resource->lock_for_read_count++;
  if (resource->read_lock_fences_enabled) {
    if (current_read_lock_fence_.get())
      current_read_lock_fence_->Set();
    resource->read_lock_fence = current_read_lock_fence_;
  }

  return resource;
}

void ResourceProvider::UnlockForRead(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());

  Resource* resource = &it->second;
  DCHECK_GT(resource->lock_for_read_count, 0);
  DCHECK_EQ(resource->exported_count, 0);
  resource->lock_for_read_count--;
  if (resource->marked_for_deletion && !resource->lock_for_read_count) {
    if (!resource->child_id) {
      // The resource belongs to this ResourceProvider, so it can be destroyed.
      DeleteResourceInternal(it, NORMAL);
    } else {
      if (batch_return_resources_) {
        batched_returning_resources_[resource->child_id].push_back(id);
      } else {
        ChildMap::iterator child_it = children_.find(resource->child_id);
        ResourceIdArray unused;
        unused.push_back(id);
        DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, unused);
      }
    }
  }
}

ResourceProvider::Resource* ResourceProvider::LockForWrite(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(CanLockForWrite(id));
  if (resource->allocated)
    WaitSyncTokenIfNeeded(id);
  resource->locked_for_write = true;
  resource->SetLocallyUsed();
  return resource;
}

bool ResourceProvider::CanLockForWrite(ResourceId id) {
  Resource* resource = GetResource(id);
  return !resource->locked_for_write && !resource->lock_for_read_count &&
         !resource->exported_count && resource->origin == Resource::INTERNAL &&
         !resource->lost && ReadLockFenceHasPassed(resource);
}

bool ResourceProvider::IsOverlayCandidate(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->is_overlay_candidate;
}

gfx::BufferFormat ResourceProvider::GetBufferFormat(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->buffer_format;
}

#if defined(OS_ANDROID)
bool ResourceProvider::IsBackedBySurfaceTexture(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->is_backed_by_surface_texture;
}

bool ResourceProvider::WantsPromotionHint(ResourceId id) {
  return wants_promotion_hints_set_.count(id) > 0;
}

size_t ResourceProvider::CountPromotionHintRequestsForTesting() {
  return wants_promotion_hints_set_.size();
}
#endif

void ResourceProvider::UnlockForWrite(Resource* resource) {
  DCHECK(resource->locked_for_write);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(resource->origin == Resource::INTERNAL);
  resource->locked_for_write = false;
}

void ResourceProvider::EnableReadLockFencesForTesting(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(resource);
  resource->read_lock_fences_enabled = true;
}

ResourceProvider::ScopedReadLockGL::ScopedReadLockGL(
    ResourceProvider* resource_provider,
    ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const Resource* resource = resource_provider->LockForRead(resource_id);
  texture_id_ = resource->gl_id;
  target_ = resource->target;
  size_ = resource->size;
  color_space_ = resource->color_space;
}

ResourceProvider::ScopedReadLockGL::~ScopedReadLockGL() {
  resource_provider_->UnlockForRead(resource_id_);
}

ResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    ResourceProvider* resource_provider,
    ResourceId resource_id,
    GLenum filter)
    : resource_lock_(resource_provider, resource_id),
      unit_(GL_TEXTURE0),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {}

ResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    ResourceProvider* resource_provider,
    ResourceId resource_id,
    GLenum unit,
    GLenum filter)
    : resource_lock_(resource_provider, resource_id),
      unit_(unit),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {}

ResourceProvider::ScopedSamplerGL::~ScopedSamplerGL() {}

ResourceProvider::ScopedWriteLockGL::ScopedWriteLockGL(
    ResourceProvider* resource_provider,
    ResourceId resource_id,
    bool create_mailbox)
    : resource_provider_(resource_provider),
      resource_id_(resource_id),
      has_sync_token_(false),
      synchronized_(false) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Resource* resource = resource_provider->LockForWrite(resource_id);
  resource_provider_->LazyAllocate(resource);
  if (resource->image_id && resource->dirty_image)
    resource_provider_->BindImageForSampling(resource);
  if (create_mailbox) {
    resource_provider_->CreateMailboxAndBindResource(
        resource_provider_->ContextGL(), resource);
  }
  texture_id_ = resource->gl_id;
  target_ = resource->target;
  format_ = resource->format;
  size_ = resource->size;
  mailbox_ = resource->mailbox();
  color_space_ = resource_provider->GetResourceColorSpaceForRaster(resource);
}

ResourceProvider::ScopedWriteLockGL::~ScopedWriteLockGL() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Resource* resource = resource_provider_->GetResource(resource_id_);
  DCHECK(resource->locked_for_write);
  // It's not sufficient to check sync_token_.HasData() here because the sync
  // might be null because of context loss. Even in that case we want to set the
  // sync token because it's checked in PrepareSendToParent while drawing.
  if (has_sync_token_)
    resource->UpdateSyncToken(sync_token_);
  if (synchronized_)
    resource->SetSynchronized();
  resource_provider_->UnlockForWrite(resource);
}

ResourceProvider::ScopedTextureProvider::ScopedTextureProvider(
    gpu::gles2::GLES2Interface* gl,
    ScopedWriteLockGL* resource_lock,
    bool use_mailbox)
    : gl_(gl), use_mailbox_(use_mailbox) {
  if (use_mailbox_) {
    texture_id_ = gl_->CreateAndConsumeTextureCHROMIUM(
        resource_lock->target(), resource_lock->mailbox().name());
  } else {
    texture_id_ = resource_lock->texture_id();
  }
  DCHECK(texture_id_);
}

ResourceProvider::ScopedTextureProvider::~ScopedTextureProvider() {
  if (use_mailbox_)
    gl_->DeleteTextures(1, &texture_id_);
}

ResourceProvider::ScopedSkSurfaceProvider::ScopedSkSurfaceProvider(
    viz::ContextProvider* context_provider,
    ScopedWriteLockGL* resource_lock,
    bool use_mailbox,
    bool use_distance_field_text,
    bool can_use_lcd_text,
    int msaa_sample_count)
    : texture_provider_(context_provider->ContextGL(),
                        resource_lock,
                        use_mailbox) {
  GrGLTextureInfo texture_info;
  texture_info.fID = texture_provider_.texture_id();
  texture_info.fTarget = resource_lock->target();
  GrBackendTexture backend_texture(
      resource_lock->size().width(), resource_lock->size().height(),
      ToGrPixelConfig(resource_lock->format()), texture_info);
  uint32_t flags =
      use_distance_field_text ? SkSurfaceProps::kUseDistanceFieldFonts_Flag : 0;
  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
  if (can_use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props =
        SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  }
  sk_surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
      context_provider->GrContext(), backend_texture, kTopLeft_GrSurfaceOrigin,
      msaa_sample_count, nullptr, &surface_props);
}

ResourceProvider::ScopedSkSurfaceProvider::~ScopedSkSurfaceProvider() {
  if (sk_surface_.get()) {
    sk_surface_->prepareForExternalIO();
    sk_surface_.reset();
  }
}

void ResourceProvider::PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                                    const Resource* resource) {
  DCHECK_EQ(viz::RGBA_8888, resource->format);
  SkImageInfo info = SkImageInfo::MakeN32Premul(resource->size.width(),
                                                resource->size.height());
  sk_bitmap->installPixels(info, resource->pixels, info.minRowBytes());
}

ResourceProvider::ScopedReadLockSoftware::ScopedReadLockSoftware(
    ResourceProvider* resource_provider,
    ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const Resource* resource = resource_provider->LockForRead(resource_id);
  resource_provider->PopulateSkBitmapWithResource(&sk_bitmap_, resource);
}

ResourceProvider::ScopedReadLockSoftware::~ScopedReadLockSoftware() {
  resource_provider_->UnlockForRead(resource_id_);
}

ResourceProvider::ScopedReadLockSkImage::ScopedReadLockSkImage(
    ResourceProvider* resource_provider,
    ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const Resource* resource = resource_provider->LockForRead(resource_id);
  if (resource->gl_id) {
    GrGLTextureInfo texture_info;
    texture_info.fID = resource->gl_id;
    texture_info.fTarget = resource->target;
    GrBackendTexture backend_texture(
        resource->size.width(), resource->size.height(),
        ToGrPixelConfig(resource->format), texture_info);
    sk_image_ = SkImage::MakeFromTexture(
        resource_provider->compositor_context_provider_->GrContext(),
        backend_texture, kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
        nullptr);
  } else if (resource->pixels) {
    SkBitmap sk_bitmap;
    resource_provider->PopulateSkBitmapWithResource(&sk_bitmap, resource);
    sk_bitmap.setImmutable();
    sk_image_ = SkImage::MakeFromBitmap(sk_bitmap);
  } else {
    // During render process shutdown, ~RenderMessageFilter which calls
    // ~HostSharedBitmapClient (which deletes shared bitmaps from child)
    // can race with OnBeginFrameDeadline which draws a frame.
    // In these cases, shared bitmaps (and this read lock) won't be valid.
    // Renderers need to silently handle locks failing until this race
    // is fixed.  DCHECK that this is the only case where there are no pixels.
    DCHECK(!resource->shared_bitmap_id.IsZero());
  }
}

ResourceProvider::ScopedReadLockSkImage::~ScopedReadLockSkImage() {
  resource_provider_->UnlockForRead(resource_id_);
}

ResourceProvider::ScopedWriteLockSoftware::ScopedWriteLockSoftware(
    ResourceProvider* resource_provider,
    ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  Resource* resource = resource_provider->LockForWrite(resource_id);
  resource_provider->PopulateSkBitmapWithResource(&sk_bitmap_, resource);
  color_space_ = resource_provider->GetResourceColorSpaceForRaster(resource);
  DCHECK(valid());
}

ResourceProvider::ScopedWriteLockSoftware::~ScopedWriteLockSoftware() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Resource* resource = resource_provider_->GetResource(resource_id_);
  DCHECK(resource);
  resource->SetSynchronized();
  resource_provider_->UnlockForWrite(resource);
}

ResourceProvider::ScopedWriteLockGpuMemoryBuffer::
    ScopedWriteLockGpuMemoryBuffer(ResourceProvider* resource_provider,
                                   ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  Resource* resource = resource_provider->LockForWrite(resource_id);
  DCHECK(IsGpuResourceType(resource->type));
  format_ = resource->format;
  size_ = resource->size;
  usage_ = resource->usage;
  gpu_memory_buffer_ = std::move(resource->gpu_memory_buffer);
  resource->gpu_memory_buffer = nullptr;
  color_space_ = resource_provider->GetResourceColorSpaceForRaster(resource);
}

ResourceProvider::ScopedWriteLockGpuMemoryBuffer::
    ~ScopedWriteLockGpuMemoryBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Resource* resource = resource_provider_->GetResource(resource_id_);
  DCHECK(resource);
  if (gpu_memory_buffer_) {
    // Note that this impacts overlay compositing, not rasterization.
    if (resource_provider_->settings_.enable_color_correct_rasterization)
      gpu_memory_buffer_->SetColorSpaceForScanout(resource->color_space);
    DCHECK(!resource->gpu_memory_buffer);
    resource_provider_->LazyCreate(resource);
    resource->gpu_memory_buffer = std::move(gpu_memory_buffer_);
    resource->allocated = true;
    resource_provider_->LazyCreateImage(resource);
    resource->dirty_image = true;
    resource->is_overlay_candidate = true;
    // GpuMemoryBuffer provides direct access to the memory used by the GPU.
    // Read lock fences are required to ensure that we're not trying to map a
    // buffer that is currently in-use by the GPU.
    resource->read_lock_fences_enabled = true;
  }
  resource->SetSynchronized();
  resource_provider_->UnlockForWrite(resource);
}

gfx::GpuMemoryBuffer*
ResourceProvider::ScopedWriteLockGpuMemoryBuffer::GetGpuMemoryBuffer() {
  if (!gpu_memory_buffer_) {
    gpu_memory_buffer_ =
        resource_provider_->gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
            size_, BufferFormat(format_), usage_, gpu::kNullSurfaceHandle);
  }
  return gpu_memory_buffer_.get();
}

ResourceProvider::ScopedBatchReturnResources::ScopedBatchReturnResources(
    ResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
  resource_provider_->SetBatchReturnResources(true);
}

ResourceProvider::ScopedBatchReturnResources::~ScopedBatchReturnResources() {
  resource_provider_->SetBatchReturnResources(false);
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

int ResourceProvider::CreateChild(const ReturnCallback& return_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Child child_info;
  child_info.return_callback = return_callback;

  int child = next_child_++;
  children_[child] = child_info;
  return child;
}

void ResourceProvider::SetChildNeedsSyncTokens(int child_id, bool needs) {
  ChildMap::iterator it = children_.find(child_id);
  DCHECK(it != children_.end());
  it->second.needs_sync_tokens = needs;
}

void ResourceProvider::DestroyChild(int child_id) {
  ChildMap::iterator it = children_.find(child_id);
  DCHECK(it != children_.end());
  DestroyChildInternal(it, NORMAL);
}

void ResourceProvider::DestroyChildInternal(ChildMap::iterator it,
                                            DeleteStyle style) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Child& child = it->second;
  DCHECK(style == FOR_SHUTDOWN || !child.marked_for_deletion);

  ResourceIdArray resources_for_child;

  for (ResourceIdMap::iterator child_it = child.child_to_parent_map.begin();
       child_it != child.child_to_parent_map.end(); ++child_it) {
    ResourceId id = child_it->second;
    resources_for_child.push_back(id);
  }

  child.marked_for_deletion = true;

  DeleteAndReturnUnusedResourcesToChild(it, style, resources_for_child);
}

const ResourceProvider::ResourceIdMap& ResourceProvider::GetChildToParentMap(
    int child) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  ChildMap::const_iterator it = children_.find(child);
  DCHECK(it != children_.end());
  DCHECK(!it->second.marked_for_deletion);
  return it->second.child_to_parent_map;
}

void ResourceProvider::PrepareSendToParent(
    const ResourceIdArray& resource_ids,
    std::vector<TransferableResource>* list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();

  // This function goes through the array multiple times, store the resources
  // as pointers so we don't have to look up the resource id multiple times.
  std::vector<Resource*> resources;
  resources.reserve(resource_ids.size());
  for (const ResourceId id : resource_ids) {
    Resource* resource = GetResource(id);
    // Check the synchronization and sync token state when delegated sync points
    // are required. The only case where we allow a sync token to not be set is
    // the case where the image is dirty. In that case we will bind the image
    // lazily and generate a sync token at that point.
    DCHECK(!settings_.delegated_sync_points_required || resource->dirty_image ||
           !resource->needs_sync_token());

    // If we are validating the resource to be sent, the resource cannot be
    // in a LOCALLY_USED state. It must have been properly synchronized.
    DCHECK(!settings_.delegated_sync_points_required ||
           Resource::LOCALLY_USED != resource->synchronization_state());

    resources.push_back(resource);
  }

  // Lazily create any mailboxes and verify all unverified sync tokens.
  std::vector<GLbyte*> unverified_sync_tokens;
  std::vector<Resource*> need_synchronization_resources;
  for (Resource* resource : resources) {
    if (!IsGpuResourceType(resource->type))
      continue;

    CreateMailboxAndBindResource(gl, resource);

    if (settings_.delegated_sync_points_required) {
      if (resource->needs_sync_token()) {
        need_synchronization_resources.push_back(resource);
      } else if (resource->mailbox().HasSyncToken() &&
                 !resource->mailbox().sync_token().verified_flush()) {
        unverified_sync_tokens.push_back(resource->GetSyncTokenData());
      }
    }
  }

  // Insert sync point to synchronize the mailbox creation or bound textures.
  gpu::SyncToken new_sync_token;
  if (!need_synchronization_resources.empty()) {
    DCHECK(settings_.delegated_sync_points_required);
    DCHECK(gl);
    const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->OrderingBarrierCHROMIUM();
    gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, new_sync_token.GetData());
    unverified_sync_tokens.push_back(new_sync_token.GetData());
  }

  if (!unverified_sync_tokens.empty()) {
    DCHECK(settings_.delegated_sync_points_required);
    DCHECK(gl);
    gl->VerifySyncTokensCHROMIUM(unverified_sync_tokens.data(),
                                 unverified_sync_tokens.size());
  }

  // Set sync token after verification.
  for (Resource* resource : need_synchronization_resources) {
    DCHECK(IsGpuResourceType(resource->type));
    resource->UpdateSyncToken(new_sync_token);
    resource->SetSynchronized();
  }

  // Transfer Resources
  DCHECK_EQ(resources.size(), resource_ids.size());
  for (size_t i = 0; i < resources.size(); ++i) {
    Resource* source = resources[i];
    const ResourceId id = resource_ids[i];

    DCHECK(!settings_.delegated_sync_points_required ||
           !source->needs_sync_token());
    DCHECK(!settings_.delegated_sync_points_required ||
           Resource::LOCALLY_USED != source->synchronization_state());

    TransferableResource resource;
    TransferResource(source, id, &resource);

    source->exported_count++;
    list->push_back(resource);
  }
}

void ResourceProvider::ReceiveFromChild(
    int child,
    const std::vector<TransferableResource>& resources) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  Child& child_info = children_.find(child)->second;
  DCHECK(!child_info.marked_for_deletion);
  for (std::vector<TransferableResource>::const_iterator it = resources.begin();
       it != resources.end(); ++it) {
    ResourceIdMap::iterator resource_in_map_it =
        child_info.child_to_parent_map.find(it->id);
    if (resource_in_map_it != child_info.child_to_parent_map.end()) {
      Resource* resource = GetResource(resource_in_map_it->second);
      resource->marked_for_deletion = false;
      resource->imported_count++;
      continue;
    }

    if ((!it->is_software && !gl) ||
        (it->is_software && !shared_bitmap_manager_)) {
      TRACE_EVENT0("cc", "ResourceProvider::ReceiveFromChild dropping invalid");
      std::vector<ReturnedResource> to_return;
      to_return.push_back(it->ToReturnedResource());
      child_info.return_callback.Run(to_return,
                                     blocking_main_thread_task_runner_);
      continue;
    }

    ResourceId local_id = next_id_++;
    Resource* resource = nullptr;
    if (it->is_software) {
      resource = InsertResource(local_id,
                                Resource(it->mailbox_holder.mailbox, it->size,
                                         Resource::DELEGATED, GL_LINEAR));
    } else {
      resource = InsertResource(
          local_id, Resource(0, it->size, Resource::DELEGATED,
                             it->mailbox_holder.texture_target, it->filter,
                             TEXTURE_HINT_IMMUTABLE, RESOURCE_TYPE_GL_TEXTURE,
                             it->format));
      resource->buffer_format = it->buffer_format;
      resource->set_mailbox(viz::TextureMailbox(
          it->mailbox_holder.mailbox, it->mailbox_holder.sync_token,
          it->mailbox_holder.texture_target));
      resource->read_lock_fences_enabled = it->read_lock_fences_enabled;
      resource->is_overlay_candidate = it->is_overlay_candidate;
#if defined(OS_ANDROID)
      resource->is_backed_by_surface_texture = it->is_backed_by_surface_texture;
      resource->wants_promotion_hint = it->wants_promotion_hint;
      if (resource->wants_promotion_hint)
        wants_promotion_hints_set_.insert(local_id);
#endif
      resource->color_space = it->color_space;
    }
    resource->child_id = child;
    // Don't allocate a texture for a child.
    resource->allocated = true;
    resource->imported_count = 1;
    resource->id_in_child = it->id;
    child_info.child_to_parent_map[it->id] = local_id;
  }
}

void ResourceProvider::DeclareUsedResourcesFromChild(
    int child,
    const ResourceIdSet& resources_from_child) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ChildMap::iterator child_it = children_.find(child);
  DCHECK(child_it != children_.end());
  Child& child_info = child_it->second;
  DCHECK(!child_info.marked_for_deletion);

  ResourceIdArray unused;
  for (ResourceIdMap::iterator it = child_info.child_to_parent_map.begin();
       it != child_info.child_to_parent_map.end(); ++it) {
    ResourceId local_id = it->second;
    bool resource_is_in_use = resources_from_child.count(it->first) > 0;
    if (!resource_is_in_use)
      unused.push_back(local_id);
  }
  DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, unused);
}

void ResourceProvider::ReceiveReturnsFromParent(
    const std::vector<ReturnedResource>& resources) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();

  std::unordered_map<int, ResourceIdArray> resources_for_child;

  for (const ReturnedResource& returned : resources) {
    ResourceId local_id = returned.id;
    ResourceMap::iterator map_iterator = resources_.find(local_id);
    // Resource was already lost (e.g. it belonged to a child that was
    // destroyed).
    if (map_iterator == resources_.end())
      continue;

    Resource* resource = &map_iterator->second;

    CHECK_GE(resource->exported_count, returned.count);
    resource->exported_count -= returned.count;
    resource->lost |= returned.lost;
    if (resource->exported_count)
      continue;

    if (returned.sync_token.HasData()) {
      DCHECK(!resource->has_shared_bitmap_id);
      if (resource->origin == Resource::INTERNAL) {
        DCHECK(resource->gl_id);
        DCHECK(returned.sync_token.HasData());
        gl->WaitSyncTokenCHROMIUM(returned.sync_token.GetConstData());
        resource->SetSynchronized();
      } else {
        DCHECK(!resource->gl_id);
        resource->UpdateSyncToken(returned.sync_token);
      }
    }

    if (!resource->marked_for_deletion)
      continue;

    if (!resource->child_id) {
      // The resource belongs to this ResourceProvider, so it can be destroyed.
      DeleteResourceInternal(map_iterator, NORMAL);
      continue;
    }

    DCHECK(resource->origin == Resource::DELEGATED);
    resources_for_child[resource->child_id].push_back(local_id);
  }

  for (const auto& children : resources_for_child) {
    ChildMap::iterator child_it = children_.find(children.first);
    DCHECK(child_it != children_.end());
    DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, children.second);
  }
}

void ResourceProvider::SetBatchReturnResources(bool batch) {
  DCHECK_NE(batch_return_resources_, batch);
  batch_return_resources_ = batch;
  if (!batch) {
    for (const auto& resources : batched_returning_resources_) {
      ChildMap::iterator child_it = children_.find(resources.first);
      DCHECK(child_it != children_.end());
      DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, resources.second);
    }
    batched_returning_resources_.clear();
  }
}

#if defined(OS_ANDROID)
void ResourceProvider::SendPromotionHints(
    const OverlayCandidateList::PromotionHintInfoMap& promotion_hints) {
  GLES2Interface* gl = ContextGL();
  if (!gl)
    return;

  for (const auto& id : wants_promotion_hints_set_) {
    const ResourceMap::iterator it = resources_.find(id);
    if (it == resources_.end())
      continue;

    if (it->second.marked_for_deletion)
      continue;

    const Resource* resource = LockForRead(id);
    DCHECK(resource->wants_promotion_hint);

    // Insist that this is backed by a GPU texture.
    if (IsGpuResourceType(resource->type)) {
      DCHECK(resource->gl_id);
      auto iter = promotion_hints.find(id);
      bool promotable = iter != promotion_hints.end();
      gl->OverlayPromotionHintCHROMIUM(resource->gl_id, promotable,
                                       promotable ? iter->second.x() : 0,
                                       promotable ? iter->second.y() : 0);
    }
    UnlockForRead(id);
  }
}
#endif

void ResourceProvider::CreateMailboxAndBindResource(
    gpu::gles2::GLES2Interface* gl,
    Resource* resource) {
  DCHECK(IsGpuResourceType(resource->type));
  DCHECK(gl);

  if (!resource->mailbox().IsValid()) {
    LazyCreate(resource);

    gpu::MailboxHolder mailbox_holder;
    mailbox_holder.texture_target = resource->target;
    gl->GenMailboxCHROMIUM(mailbox_holder.mailbox.name);
    gl->ProduceTextureDirectCHROMIUM(resource->gl_id,
                                     mailbox_holder.texture_target,
                                     mailbox_holder.mailbox.name);
    resource->set_mailbox(viz::TextureMailbox(mailbox_holder));
  }

  if (resource->image_id && resource->dirty_image) {
    DCHECK(resource->gl_id);
    DCHECK(resource->origin == Resource::INTERNAL);
    BindImageForSampling(resource);
  }
}

void ResourceProvider::TransferResource(Resource* source,
                                        ResourceId id,
                                        TransferableResource* resource) {
  DCHECK(!source->locked_for_write);
  DCHECK(!source->lock_for_read_count);
  DCHECK(source->origin != Resource::EXTERNAL || source->mailbox().IsValid());
  DCHECK(source->allocated);
  resource->id = id;
  resource->format = source->format;
  resource->buffer_format = source->buffer_format;
  resource->mailbox_holder.texture_target = source->target;
  resource->filter = source->filter;
  resource->size = source->size;
  resource->read_lock_fences_enabled = source->read_lock_fences_enabled;
  resource->is_overlay_candidate = source->is_overlay_candidate;
#if defined(OS_ANDROID)
  resource->is_backed_by_surface_texture = source->is_backed_by_surface_texture;
  resource->wants_promotion_hint = source->wants_promotion_hint;
#endif
  resource->color_space = source->color_space;

  if (source->type == RESOURCE_TYPE_BITMAP) {
    resource->mailbox_holder.mailbox = source->shared_bitmap_id;
    resource->is_software = true;
    if (source->shared_bitmap) {
      resource->shared_bitmap_sequence_number =
          source->shared_bitmap->sequence_number();
    } else {
      resource->shared_bitmap_sequence_number = 0;
    }
  } else {
    DCHECK(source->mailbox().IsValid());
    DCHECK(source->mailbox().IsTexture());
    DCHECK(!source->image_id || !source->dirty_image);
    // This is either an external resource, or a compositor resource that we
    // already exported. Make sure to forward the sync point that we were given.
    resource->mailbox_holder.mailbox = source->mailbox().mailbox();
    resource->mailbox_holder.texture_target = source->mailbox().target();
    resource->mailbox_holder.sync_token = source->mailbox().sync_token();
  }
}

void ResourceProvider::DeleteAndReturnUnusedResourcesToChild(
    ChildMap::iterator child_it,
    DeleteStyle style,
    const ResourceIdArray& unused) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(child_it != children_.end());
  Child* child_info = &child_it->second;

  if (unused.empty() && !child_info->marked_for_deletion)
    return;

  std::vector<ReturnedResource> to_return;
  to_return.reserve(unused.size());
  std::vector<ReturnedResource*> need_synchronization_resources;
  std::vector<GLbyte*> unverified_sync_tokens;

  GLES2Interface* gl = ContextGL();

  for (ResourceId local_id : unused) {
    ResourceMap::iterator it = resources_.find(local_id);
    CHECK(it != resources_.end());
    Resource& resource = it->second;

    DCHECK(!resource.locked_for_write);

    ResourceId child_id = resource.id_in_child;
    DCHECK(child_info->child_to_parent_map.count(child_id));

    bool is_lost = resource.lost ||
                   (IsGpuResourceType(resource.type) && lost_context_provider_);
    if (resource.exported_count > 0 || resource.lock_for_read_count > 0) {
      if (style != FOR_SHUTDOWN) {
        // Defer this resource deletion.
        resource.marked_for_deletion = true;
        continue;
      }
      // We can't postpone the deletion, so we'll have to lose it.
      is_lost = true;
    } else if (!ReadLockFenceHasPassed(&resource)) {
      // TODO(dcastagna): see if it's possible to use this logic for
      // the branch above too, where the resource is locked or still exported.
      if (style != FOR_SHUTDOWN && !child_info->marked_for_deletion) {
        // Defer this resource deletion.
        resource.marked_for_deletion = true;
        continue;
      }
      // We can't postpone the deletion, so we'll have to lose it.
      is_lost = true;
    }

    if (IsGpuResourceType(resource.type) &&
        resource.filter != resource.original_filter) {
      DCHECK(resource.target);
      DCHECK(resource.gl_id);
      DCHECK(gl);
      gl->BindTexture(resource.target, resource.gl_id);
      gl->TexParameteri(resource.target, GL_TEXTURE_MIN_FILTER,
                        resource.original_filter);
      gl->TexParameteri(resource.target, GL_TEXTURE_MAG_FILTER,
                        resource.original_filter);
      resource.SetLocallyUsed();
    }

    ReturnedResource returned;
    returned.id = child_id;
    returned.sync_token = resource.mailbox().sync_token();
    returned.count = resource.imported_count;
    returned.lost = is_lost;
    to_return.push_back(returned);

    if (IsGpuResourceType(resource.type) && child_info->needs_sync_tokens) {
      if (resource.needs_sync_token()) {
        need_synchronization_resources.push_back(&to_return.back());
      } else if (returned.sync_token.HasData() &&
                 !returned.sync_token.verified_flush()) {
        // Before returning any sync tokens, they must be verified.
        unverified_sync_tokens.push_back(returned.sync_token.GetData());
      }
    }

    child_info->child_to_parent_map.erase(child_id);
    resource.imported_count = 0;
    DeleteResourceInternal(it, style);
  }

  gpu::SyncToken new_sync_token;
  if (!need_synchronization_resources.empty()) {
    DCHECK(child_info->needs_sync_tokens);
    DCHECK(gl);
    const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->OrderingBarrierCHROMIUM();
    gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, new_sync_token.GetData());
    unverified_sync_tokens.push_back(new_sync_token.GetData());
  }

  if (!unverified_sync_tokens.empty()) {
    DCHECK(child_info->needs_sync_tokens);
    DCHECK(gl);
    gl->VerifySyncTokensCHROMIUM(unverified_sync_tokens.data(),
                                 unverified_sync_tokens.size());
  }

  // Set sync token after verification.
  for (ReturnedResource* returned : need_synchronization_resources)
    returned->sync_token = new_sync_token;

  if (!to_return.empty())
    child_info->return_callback.Run(to_return,
                                    blocking_main_thread_task_runner_);

  if (child_info->marked_for_deletion &&
      child_info->child_to_parent_map.empty()) {
    children_.erase(child_it);
  }
}

GLenum ResourceProvider::BindForSampling(ResourceId resource_id,
                                         GLenum unit,
                                         GLenum filter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  ResourceMap::iterator it = resources_.find(resource_id);
  DCHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(resource->lock_for_read_count);
  DCHECK(!resource->locked_for_write);

  ScopedSetActiveTexture scoped_active_tex(gl, unit);
  GLenum target = resource->target;
  gl->BindTexture(target, resource->gl_id);
  if (filter != resource->filter) {
    gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    resource->filter = filter;
  }

  if (resource->image_id && resource->dirty_image)
    BindImageForSampling(resource);

  return target;
}

void ResourceProvider::CreateForTesting(ResourceId id) {
  LazyCreate(GetResource(id));
}

void ResourceProvider::LazyCreate(Resource* resource) {
  if (!IsGpuResourceType(resource->type) ||
      resource->origin != Resource::INTERNAL)
    return;

  if (resource->gl_id)
    return;

  DCHECK(resource->origin == Resource::INTERNAL);
  DCHECK(!resource->mailbox().IsValid());
  resource->gl_id = texture_id_allocator_->NextId();

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
      (resource->hint & TEXTURE_HINT_FRAMEBUFFER)) {
    gl->TexParameteri(resource->target, GL_TEXTURE_USAGE_ANGLE,
                      GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  }
}

void ResourceProvider::AllocateForTesting(ResourceId id) {
  LazyAllocate(GetResource(id));
}

void ResourceProvider::LazyAllocate(Resource* resource) {
  DCHECK(resource);
  if (resource->allocated)
    return;
  LazyCreate(resource);
  if (!resource->gl_id)
    return;
  resource->allocated = true;
  GLES2Interface* gl = ContextGL();
  gfx::Size& size = resource->size;
  viz::ResourceFormat format = resource->format;
  gl->BindTexture(resource->target, resource->gl_id);
  if (resource->type == RESOURCE_TYPE_GPU_MEMORY_BUFFER) {
    resource->gpu_memory_buffer =
        gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
            size, BufferFormat(format), resource->usage,
            gpu::kNullSurfaceHandle);
    // Note that this impacts overlay compositing, not rasterization.
    if (resource->gpu_memory_buffer &&
        settings_.enable_color_correct_rasterization) {
      resource->gpu_memory_buffer->SetColorSpaceForScanout(
          resource->color_space);
    }

    LazyCreateImage(resource);
    resource->dirty_image = true;
    resource->is_overlay_candidate = true;
    // GpuMemoryBuffer provides direct access to the memory used by the GPU.
    // Read lock fences are required to ensure that we're not trying to map a
    // buffer that is currently in-use by the GPU.
    resource->read_lock_fences_enabled = true;
  } else if (settings_.use_texture_storage_ext &&
             IsFormatSupportedForStorage(format,
                                         settings_.use_texture_format_bgra) &&
             (resource->hint & TEXTURE_HINT_IMMUTABLE)) {
    GLenum storage_format = TextureToStorageFormat(format);
    gl->TexStorage2DEXT(resource->target, 1, storage_format, size.width(),
                        size.height());
  } else {
    // viz::ETC1 does not support preallocation.
    if (format != viz::ETC1) {
      gl->TexImage2D(resource->target, 0, GLInternalFormat(format),
                     size.width(), size.height(), 0, GLDataFormat(format),
                     GLDataType(format), nullptr);
    }
  }
}

void ResourceProvider::LazyCreateImage(Resource* resource) {
  DCHECK(resource->gpu_memory_buffer);
  DCHECK(resource->gl_id);
  DCHECK(resource->allocated);
  // Avoid crashing in release builds if GpuMemoryBuffer allocation fails.
  // http://crbug.com/554541
  if (!resource->gpu_memory_buffer)
    return;
  if (!resource->image_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
    // TODO(reveman): This avoids a performance problem on ARM ChromeOS
    // devices. This only works with shared memory backed buffers.
    // crbug.com/580166
    DCHECK_EQ(resource->gpu_memory_buffer->GetHandle().type,
              gfx::SHARED_MEMORY_BUFFER);
#endif
    resource->image_id = gl->CreateImageCHROMIUM(
        resource->gpu_memory_buffer->AsClientBuffer(), resource->size.width(),
        resource->size.height(), GLInternalFormat(resource->format));
    DCHECK(resource->image_id || IsGLContextLost());

    resource->SetLocallyUsed();
  }
}

void ResourceProvider::BindImageForSampling(Resource* resource) {
  GLES2Interface* gl = ContextGL();
  DCHECK(resource->gl_id);
  DCHECK(resource->image_id);

  // Release image currently bound to texture.
  gl->BindTexture(resource->target, resource->gl_id);
  if (resource->bound_image_id)
    gl->ReleaseTexImage2DCHROMIUM(resource->target, resource->bound_image_id);
  gl->BindTexImage2DCHROMIUM(resource->target, resource->image_id);
  resource->bound_image_id = resource->image_id;
  resource->dirty_image = false;
  resource->SetLocallyUsed();
}

void ResourceProvider::WaitSyncTokenIfNeeded(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(resource->allocated);
  if (Resource::NEEDS_WAIT == resource->synchronization_state()) {
    DCHECK(IsGpuResourceType(resource->type));

    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    resource->WaitSyncToken(gl);
  }
}

GLint ResourceProvider::GetActiveTextureUnit(GLES2Interface* gl) {
  GLint active_unit = 0;
  gl->GetIntegerv(GL_ACTIVE_TEXTURE, &active_unit);
  return active_unit;
}

GLenum ResourceProvider::GetImageTextureTarget(gfx::BufferUsage usage,
                                               viz::ResourceFormat format) {
  gfx::BufferFormat buffer_format = BufferFormat(format);
  auto found = buffer_to_texture_target_map_.find(
      viz::BufferToTextureTargetKey(usage, buffer_format));
  DCHECK(found != buffer_to_texture_target_map_.end());
  return found->second;
}

void ResourceProvider::ValidateResource(ResourceId id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(id);
  DCHECK(resources_.find(id) != resources_.end());
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
  DCHECK(thread_checker_.CalledOnValidThread());

  const uint64_t tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();

  for (const auto& resource_entry : resources_) {
    const auto& resource = resource_entry.second;

    bool backing_memory_allocated = false;
    switch (resource.type) {
      case RESOURCE_TYPE_GPU_MEMORY_BUFFER:
        backing_memory_allocated = !!resource.gpu_memory_buffer;
        break;
      case RESOURCE_TYPE_GL_TEXTURE:
        backing_memory_allocated = !!resource.gl_id;
        break;
      case RESOURCE_TYPE_BITMAP:
        backing_memory_allocated = resource.has_shared_bitmap_id;
        break;
    }

    if (!backing_memory_allocated) {
      // Don't log unallocated resources - they have no backing memory.
      continue;
    }

    // Resource IDs are not process-unique, so log with the ResourceProvider's
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
      case RESOURCE_TYPE_GPU_MEMORY_BUFFER:
        guid =
            resource.gpu_memory_buffer->GetGUIDForTracing(tracing_process_id);
        shared_memory_guid =
            resource.gpu_memory_buffer->GetHandle().handle.GetGUID();
        break;
      case RESOURCE_TYPE_GL_TEXTURE:
        DCHECK(resource.gl_id);
        guid = gl::GetGLTextureClientGUIDForTracing(
            compositor_context_provider_->ContextSupport()
                ->ShareGroupTracingGUID(),
            resource.gl_id);
        break;
      case RESOURCE_TYPE_BITMAP:
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
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), guid,
                                           shared_memory_guid, kImportance);
    } else {
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  return true;
}

}  // namespace cc

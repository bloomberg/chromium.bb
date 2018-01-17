// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/layer_tree_resource_provider.h"

#include "base/bits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/resources/resource_util.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "third_party/skia/include/core/SkCanvas.h"

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
    if (gl_)
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

LayerTreeResourceProvider::Settings::Settings(
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

namespace {
// The resource id in LayerTreeResourceProvider starts from 1 to avoid
// conflicts with id from DisplayResourceProvider.
const unsigned int kLayerTreeInitialResourceId = 1;
}  // namespace

struct LayerTreeResourceProvider::ImportedResource {
  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  int exported_count = 0;
  bool marked_for_deletion = false;

  gpu::SyncToken returned_sync_token;
  bool returned_lost = false;

  ImportedResource(viz::ResourceId id,
                   const viz::TransferableResource& resource,
                   std::unique_ptr<viz::SingleReleaseCallback> release_callback)
      : resource(resource), release_callback(std::move(release_callback)) {
    // Replace the |resource| id with the local id from this
    // LayerTreeResourceProvider.
    this->resource.id = id;
  }
  ~ImportedResource() = default;

  ImportedResource(ImportedResource&&) = default;
  ImportedResource& operator=(ImportedResource&&) = default;
};

LayerTreeResourceProvider::LayerTreeResourceProvider(
    viz::ContextProvider* compositor_context_provider,
    viz::SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool delegated_sync_points_required,
    const viz::ResourceSettings& resource_settings)
    : ResourceProvider(compositor_context_provider),
      settings_(compositor_context_provider,
                delegated_sync_points_required,
                resource_settings),
      shared_bitmap_manager_(shared_bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      next_id_(kLayerTreeInitialResourceId) {
  DCHECK(resource_settings.texture_id_allocation_chunk_size);
  GLES2Interface* gl = ContextGL();
  texture_id_allocator_ = std::make_unique<TextureIdAllocator>(
      gl, resource_settings.texture_id_allocation_chunk_size);
}

LayerTreeResourceProvider::~LayerTreeResourceProvider() {
  for (auto& pair : imported_resources_) {
    ImportedResource& imported = pair.second;
    // If the resource is exported we can't report when it can be used again
    // once this class is destroyed, so consider the resource lost.
    bool is_lost = imported.exported_count || imported.returned_lost;
    imported.release_callback->Run(imported.returned_sync_token, is_lost);
  }
  texture_id_allocator_ = nullptr;
  GLES2Interface* gl = ContextGL();
  if (gl)
    gl->Finish();
}

gpu::SyncToken LayerTreeResourceProvider::GenerateSyncTokenHelper(
    gpu::gles2::GLES2Interface* gl) {
  DCHECK(gl);
  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  DCHECK(sync_token.HasData() ||
         gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  return sync_token;
}

gpu::SyncToken LayerTreeResourceProvider::GenerateSyncTokenHelper(
    gpu::raster::RasterInterface* ri) {
  DCHECK(ri);
  gpu::SyncToken sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  DCHECK(sync_token.HasData() ||
         ri->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  return sync_token;
}

gpu::SyncToken LayerTreeResourceProvider::GetSyncTokenForResources(
    const ResourceIdArray& resource_ids) {
  gpu::SyncToken latest_sync_token;
  for (viz::ResourceId id : resource_ids) {
    const gpu::SyncToken& sync_token = GetResource(id)->sync_token();
    if (sync_token.release_count() > latest_sync_token.release_count())
      latest_sync_token = sync_token;
  }
  return latest_sync_token;
}

gfx::ColorSpace LayerTreeResourceProvider::GetResourceColorSpaceForRaster(
    const viz::internal::Resource* resource) const {
  return resource->color_space;
}

void LayerTreeResourceProvider::PrepareSendToParent(
    const ResourceIdArray& export_ids,
    std::vector<viz::TransferableResource>* list) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GLES2Interface* gl = ContextGL();

  // This function goes through the array multiple times, store the resources
  // as pointers so we don't have to look up the resource id multiple times.
  // Make sure the maps do not change while these vectors are alive or they
  // will become invalid.
  std::vector<std::pair<viz::internal::Resource*, viz::ResourceId>> resources;
  std::vector<ImportedResource*> imports;
  resources.reserve(export_ids.size());
  imports.reserve(export_ids.size());
  for (const viz::ResourceId id : export_ids) {
    auto it = imported_resources_.find(id);
    if (it != imported_resources_.end())
      imports.push_back(&it->second);
    else
      resources.push_back({GetResource(id), id});
  }
  DCHECK_EQ(resources.size() + imports.size(), export_ids.size());

  // Lazily create any mailboxes and verify all unverified sync tokens.
  std::vector<GLbyte*> unverified_sync_tokens;
  std::vector<viz::internal::Resource*> need_synchronization_resources;
  for (auto& pair : resources) {
    viz::internal::Resource* resource = pair.first;
    if (!resource->is_gpu_resource_type())
      continue;

    CreateMailbox(resource);

    if (settings_.delegated_sync_points_required) {
      if (resource->needs_sync_token()) {
        need_synchronization_resources.push_back(resource);
      } else if (!resource->sync_token().verified_flush()) {
        unverified_sync_tokens.push_back(resource->GetSyncTokenData());
      }
    }
  }
  if (settings_.delegated_sync_points_required) {
    for (ImportedResource* imported : imports) {
      if (!imported->resource.is_software &&
          !imported->resource.mailbox_holder.sync_token.verified_flush()) {
        unverified_sync_tokens.push_back(
            imported->resource.mailbox_holder.sync_token.GetData());
      }
    }
  }

  // Insert sync point to synchronize the mailbox creation or bound textures.
  gpu::SyncToken new_sync_token;
  if (!need_synchronization_resources.empty()) {
    DCHECK(settings_.delegated_sync_points_required);
    DCHECK(gl);
    gl->GenUnverifiedSyncTokenCHROMIUM(new_sync_token.GetData());
    unverified_sync_tokens.push_back(new_sync_token.GetData());
  }

  if (compositor_context_provider_)
    compositor_context_provider_->ContextSupport()->FlushPendingWork();

  if (!unverified_sync_tokens.empty()) {
    DCHECK(settings_.delegated_sync_points_required);
    DCHECK(gl);
    gl->VerifySyncTokensCHROMIUM(unverified_sync_tokens.data(),
                                 unverified_sync_tokens.size());
  }

  // Set sync token after verification.
  for (viz::internal::Resource* resource : need_synchronization_resources) {
    DCHECK(resource->is_gpu_resource_type());
    resource->UpdateSyncToken(new_sync_token);
    resource->SetSynchronized();
  }

  // Transfer Resources.
  for (size_t i = 0; i < resources.size(); ++i) {
    viz::internal::Resource* source = resources[i].first;
    const viz::ResourceId id = resources[i].second;

    DCHECK(!settings_.delegated_sync_points_required ||
           !source->needs_sync_token());
    DCHECK(!settings_.delegated_sync_points_required ||
           viz::internal::Resource::LOCALLY_USED !=
               source->synchronization_state());

    viz::TransferableResource resource;
    TransferResource(source, id, &resource);

    source->exported_count++;
    list->push_back(std::move(resource));
  }
  for (ImportedResource* imported : imports) {
    list->push_back(imported->resource);
    imported->exported_count++;
  }
}

void LayerTreeResourceProvider::ReceiveReturnsFromParent(
    const std::vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GLES2Interface* gl = ContextGL();

  for (const viz::ReturnedResource& returned : resources) {
    viz::ResourceId local_id = returned.id;

    auto import_it = imported_resources_.find(local_id);
    if (import_it != imported_resources_.end()) {
      ImportedResource& imported = import_it->second;

      DCHECK_GE(imported.exported_count, returned.count);
      imported.exported_count -= returned.count;
      imported.returned_lost |= returned.lost;

      if (imported.exported_count)
        continue;

      if (returned.sync_token.HasData()) {
        DCHECK(!imported.resource.is_software);
        imported.returned_sync_token = returned.sync_token;
      }

      if (imported.marked_for_deletion) {
        imported.release_callback->Run(imported.returned_sync_token,
                                       imported.returned_lost);
        imported_resources_.erase(import_it);
      }

      continue;
    }

    auto map_iterator = resources_.find(local_id);
    DCHECK(map_iterator != resources_.end());
    // Resource was already lost (e.g. it belonged to a child that was
    // destroyed).
    // TODO(danakj): Remove this. There is no "child" here anymore, and
    // lost resources are still in the map until exported_count == 0.
    if (map_iterator == resources_.end())
      continue;

    viz::internal::Resource* resource = &map_iterator->second;

    DCHECK_GE(resource->exported_count, returned.count);
    resource->exported_count -= returned.count;
    resource->lost |= returned.lost;
    if (resource->exported_count)
      continue;

    if (returned.sync_token.HasData()) {
      DCHECK(!resource->has_shared_bitmap_id);
      if (resource->origin == viz::internal::Resource::INTERNAL) {
        DCHECK(resource->gl_id);
        gl->WaitSyncTokenCHROMIUM(returned.sync_token.GetConstData());
        resource->SetSynchronized();
      } else {
        DCHECK(!resource->gl_id);
        resource->UpdateSyncToken(returned.sync_token);
      }
    }

    if (!resource->marked_for_deletion)
      continue;

    // The resource belongs to this LayerTreeResourceProvider, so it can be
    // destroyed.
    DeleteResourceInternal(map_iterator, NORMAL);
  }
}

viz::ResourceId LayerTreeResourceProvider::CreateGpuTextureResource(
    const gfx::Size& size,
    viz::ResourceTextureHint hint,
    viz::ResourceFormat format,
    const gfx::ColorSpace& color_space) {
  DCHECK(compositor_context_provider_);
  DCHECK(!size.IsEmpty());
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
    resource->target = GetImageTextureTarget(
        compositor_context_provider_->ContextCapabilities(), resource->usage,
        format);
    resource->buffer_format = BufferFormat(format);
    resource->is_overlay_candidate = true;
  }
  return id;
}

viz::ResourceId LayerTreeResourceProvider::CreateGpuMemoryBufferResource(
    const gfx::Size& size,
    viz::ResourceTextureHint hint,
    viz::ResourceFormat format,
    gfx::BufferUsage usage,
    const gfx::ColorSpace& color_space) {
  DCHECK(compositor_context_provider_);
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
  resource->target = GetImageTextureTarget(
      compositor_context_provider_->ContextCapabilities(), usage, format);
  resource->buffer_format = BufferFormat(format);
  resource->usage = usage;
  resource->is_overlay_candidate = true;
  // GpuMemoryBuffer provides direct access to the memory used by the GPU. Read
  // lock fences are required to ensure that we're not trying to map a buffer
  // that is currently in-use by the GPU.
  resource->read_lock_fences_enabled = true;
  return id;
}

viz::ResourceId LayerTreeResourceProvider::CreateBitmapResource(
    const gfx::Size& size,
    const gfx::ColorSpace& color_space) {
  DCHECK(!compositor_context_provider_);
  DCHECK(!size.IsEmpty());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(danakj): Allocate this outside ResourceProvider.
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

void LayerTreeResourceProvider::DeleteResource(viz::ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  viz::internal::Resource* resource = &it->second;
  DCHECK(!resource->marked_for_deletion);
  DCHECK_EQ(resource->imported_count, 0);
  DCHECK(!resource->locked_for_write);

  if (resource->exported_count > 0) {
    resource->marked_for_deletion = true;
    return;
  } else {
    DeleteResourceInternal(it, NORMAL);
  }
}

viz::ResourceId LayerTreeResourceProvider::ImportResource(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  viz::ResourceId id = next_id_++;
  auto result = imported_resources_.emplace(
      id, ImportedResource(id, resource, std::move(release_callback)));
  DCHECK(result.second);  // If false, the id was already in the map.
  return id;
}

void LayerTreeResourceProvider::RemoveImportedResource(viz::ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = imported_resources_.find(id);
  DCHECK(it != imported_resources_.end());
  ImportedResource& imported = it->second;
  imported.marked_for_deletion = true;
  if (imported.exported_count == 0) {
    imported.release_callback->Run(imported.returned_sync_token,
                                   imported.returned_lost);
    imported_resources_.erase(it);
  }
}

void LayerTreeResourceProvider::CopyToResource(viz::ResourceId id,
                                               const uint8_t* image,
                                               const gfx::Size& image_size) {
  viz::internal::Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write);
  DCHECK_EQ(resource->origin, viz::internal::Resource::INTERNAL);
  DCHECK_NE(resource->synchronization_state(),
            viz::internal::Resource::NEEDS_WAIT);
  DCHECK_EQ(resource->exported_count, 0);

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

void LayerTreeResourceProvider::CreateForTesting(viz::ResourceId id) {
  CreateTexture(GetResource(id));
}

void LayerTreeResourceProvider::CreateTexture(
    viz::internal::Resource* resource) {
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

void LayerTreeResourceProvider::CreateMailbox(
    viz::internal::Resource* resource) {
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
  gl->ProduceTextureDirectCHROMIUM(resource->gl_id, resource->mailbox.name);
  resource->SetLocallyUsed();
}

void LayerTreeResourceProvider::CreateAndBindImage(
    viz::internal::Resource* resource) {
  DCHECK(resource->gpu_memory_buffer);
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
  // TODO(reveman): This avoids a performance problem on ARM ChromeOS
  // devices. This only works with shared memory backed buffers.
  // https://crbug.com/580166
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

void LayerTreeResourceProvider::AllocateForTesting(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  if (!resource->allocated) {
    // Software and external resources are marked allocated on creation.
    DCHECK(resource->is_gpu_resource_type());
    DCHECK_EQ(resource->origin, viz::internal::Resource::INTERNAL);
    ScopedWriteLockGL resource_lock(this, id);
    resource_lock.GetTexture();  // Allocates texture.
  }
}

void LayerTreeResourceProvider::TransferResource(
    viz::internal::Resource* source,
    viz::ResourceId id,
    viz::TransferableResource* resource) {
  DCHECK(!source->locked_for_write);
  DCHECK(source->allocated);
  resource->id = id;
  resource->format = source->format;
  resource->buffer_format = source->buffer_format;
  resource->filter = source->filter;
  resource->size = source->size;
  resource->read_lock_fences_enabled = source->read_lock_fences_enabled;
  resource->is_overlay_candidate = source->is_overlay_candidate;
  resource->color_space = source->color_space;

  if (source->type == viz::ResourceType::kBitmap) {
    DCHECK(source->shared_bitmap);
    resource->mailbox_holder.mailbox = source->shared_bitmap_id;
    resource->is_software = true;
    resource->shared_bitmap_sequence_number =
        source->shared_bitmap->sequence_number();
  } else {
    DCHECK(!source->mailbox.IsZero());
    // This is either an external resource, or a compositor resource that we
    // already exported. Make sure to forward the sync point that we were given.
    resource->mailbox_holder.mailbox = source->mailbox;
    resource->mailbox_holder.texture_target = source->target;
    resource->mailbox_holder.sync_token = source->sync_token();
  }
}

bool LayerTreeResourceProvider::CanLockForWrite(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return !resource->locked_for_write && !resource->exported_count &&
         resource->origin == viz::internal::Resource::INTERNAL &&
         !resource->lost;
}

viz::internal::Resource* LayerTreeResourceProvider::LockForWrite(
    viz::ResourceId id) {
  DCHECK(CanLockForWrite(id));
  viz::internal::Resource* resource = GetResource(id);
  WaitSyncTokenInternal(resource);
  resource->SetLocallyUsed();
  resource->locked_for_write = true;
  resource->mipmap_state = viz::internal::Resource::INVALID;
  return resource;
}

void LayerTreeResourceProvider::UnlockForWrite(
    viz::internal::Resource* resource) {
  DCHECK(resource->locked_for_write);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(resource->origin == viz::internal::Resource::INTERNAL);
  resource->locked_for_write = false;
}

void LayerTreeResourceProvider::FlushPendingDeletions() const {
  if (auto* gl = ContextGL())
    gl->ShallowFlushCHROMIUM();
}

GLenum LayerTreeResourceProvider::GetImageTextureTarget(
    const gpu::Capabilities& caps,
    gfx::BufferUsage usage,
    viz::ResourceFormat format) const {
  return gpu::GetBufferTextureTarget(usage, BufferFormat(format), caps);
}

bool LayerTreeResourceProvider::IsTextureFormatSupported(
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

bool LayerTreeResourceProvider::IsRenderBufferFormatSupported(
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

bool LayerTreeResourceProvider::IsGpuMemoryBufferFormatSupported(
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
      return true;
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

viz::ResourceFormat LayerTreeResourceProvider::YuvResourceFormat(
    int bits) const {
  if (bits > 8) {
    return settings_.yuv_highbit_resource_format;
  } else {
    return settings_.yuv_resource_format;
  }
}

LayerTreeResourceProvider::ScopedWriteLockGpu::ScopedWriteLockGpu(
    LayerTreeResourceProvider* resource_provider,
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

LayerTreeResourceProvider::ScopedWriteLockGpu::~ScopedWriteLockGpu() {
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

GrPixelConfig LayerTreeResourceProvider::ScopedWriteLockGpu::PixelConfig()
    const {
  return ToGrPixelConfig(format_);
}

void LayerTreeResourceProvider::ScopedWriteLockGpu::CreateMailbox() {
  if (!mailbox_.IsZero())
    return;
  gpu::gles2::GLES2Interface* gl = resource_provider_->ContextGL();
  DCHECK(gl);
  gl->GenMailboxCHROMIUM(mailbox_.name);
  gl->ProduceTextureDirectCHROMIUM(texture_id_, mailbox_.name);
}

LayerTreeResourceProvider::ScopedWriteLockGL::ScopedWriteLockGL(
    LayerTreeResourceProvider* resource_provider,
    viz::ResourceId resource_id)
    : ScopedWriteLockGpu(resource_provider, resource_id) {}

LayerTreeResourceProvider::ScopedWriteLockGL::~ScopedWriteLockGL() {}

GLuint LayerTreeResourceProvider::ScopedWriteLockGL::GetTexture() {
  LazyAllocate(resource_provider_->ContextGL(), texture_id_);
  return texture_id_;
}

void LayerTreeResourceProvider::ScopedWriteLockGL::LazyAllocate(
    gpu::gles2::GLES2Interface* gl,
    GLuint texture_id) {
  // ETC1 resources cannot be preallocated.
  if (format_ == viz::ETC1)
    return;

  if (allocated_)
    return;
  allocated_ = true;

  const Settings& settings = resource_provider_->settings_;

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

LayerTreeResourceProvider::ScopedWriteLockRaster::ScopedWriteLockRaster(
    LayerTreeResourceProvider* resource_provider,
    viz::ResourceId resource_id)
    : ScopedWriteLockGpu(resource_provider, resource_id) {}

LayerTreeResourceProvider::ScopedWriteLockRaster::~ScopedWriteLockRaster() {}

GLuint LayerTreeResourceProvider::ScopedWriteLockRaster::ConsumeTexture(
    gpu::raster::RasterInterface* ri) {
  DCHECK(ri);
  DCHECK(!mailbox_.IsZero());

  GLuint texture_id = ri->CreateAndConsumeTextureCHROMIUM(mailbox_.name);
  DCHECK(texture_id);

  LazyAllocate(ri, texture_id);

  return texture_id;
}

void LayerTreeResourceProvider::ScopedWriteLockRaster::LazyAllocate(
    gpu::raster::RasterInterface* ri,
    GLuint texture_id) {
  // ETC1 resources cannot be preallocated.
  if (format_ == viz::ETC1)
    return;

  if (allocated_)
    return;
  allocated_ = true;

  ri->BindTexture(target_, texture_id);
  ri->TexStorageForRaster(
      target_, format_, size_.width(), size_.height(),
      is_overlay_ ? gpu::raster::kOverlay : gpu::raster::kNone);
  if (is_overlay_ && color_space_.IsValid()) {
    ri->SetColorSpaceMetadataCHROMIUM(
        texture_id, reinterpret_cast<GLColorSpace>(&color_space_));
  }
}

LayerTreeResourceProvider::ScopedWriteLockGpuMemoryBuffer ::
    ScopedWriteLockGpuMemoryBuffer(LayerTreeResourceProvider* resource_provider,
                                   viz::ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  viz::internal::Resource* resource =
      resource_provider->LockForWrite(resource_id);
  DCHECK_EQ(resource->type, viz::ResourceType::kGpuMemoryBuffer);
  size_ = resource->size;
  format_ = resource->format;
  usage_ = resource->usage;
  color_space_ = resource_provider->GetResourceColorSpaceForRaster(resource);
  gpu_memory_buffer_ = std::move(resource->gpu_memory_buffer);
}

LayerTreeResourceProvider::ScopedWriteLockGpuMemoryBuffer::
    ~ScopedWriteLockGpuMemoryBuffer() {
  viz::internal::Resource* resource =
      resource_provider_->GetResource(resource_id_);
  // Avoid crashing in release builds if GpuMemoryBuffer allocation fails.
  // http://crbug.com/554541
  if (gpu_memory_buffer_) {
    resource->gpu_memory_buffer = std::move(gpu_memory_buffer_);
    resource->allocated = true;
    resource_provider_->CreateAndBindImage(resource);
  }
  resource_provider_->UnlockForWrite(resource);
}

gfx::GpuMemoryBuffer* LayerTreeResourceProvider::
    ScopedWriteLockGpuMemoryBuffer::GetGpuMemoryBuffer() {
  if (!gpu_memory_buffer_) {
    gpu_memory_buffer_ =
        resource_provider_->gpu_memory_buffer_manager()->CreateGpuMemoryBuffer(
            size_, BufferFormat(format_), usage_, gpu::kNullSurfaceHandle);
    // Avoid crashing in release builds if GpuMemoryBuffer allocation fails.
    // http://crbug.com/554541
    if (gpu_memory_buffer_ && color_space_.IsValid())
      gpu_memory_buffer_->SetColorSpace(color_space_);
  }
  return gpu_memory_buffer_.get();
}

LayerTreeResourceProvider::ScopedWriteLockSoftware::ScopedWriteLockSoftware(
    LayerTreeResourceProvider* resource_provider,
    viz::ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  viz::internal::Resource* resource =
      resource_provider->LockForWrite(resource_id);
  resource_provider->PopulateSkBitmapWithResource(&sk_bitmap_, resource);
  color_space_ = resource_provider->GetResourceColorSpaceForRaster(resource);
  DCHECK(valid());
}

LayerTreeResourceProvider::ScopedWriteLockSoftware::~ScopedWriteLockSoftware() {
  viz::internal::Resource* resource =
      resource_provider_->GetResource(resource_id_);
  resource->SetSynchronized();
  resource_provider_->UnlockForWrite(resource);
}

LayerTreeResourceProvider::ScopedSkSurface::ScopedSkSurface(
    GrContext* gr_context,
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

LayerTreeResourceProvider::ScopedSkSurface::~ScopedSkSurface() {
  if (surface_)
    surface_->prepareForExternalIO();
}

void LayerTreeResourceProvider::ValidateResource(viz::ResourceId id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(id);
  DCHECK(resources_.find(id) != resources_.end() ||
         imported_resources_.find(id) != imported_resources_.end());
}

bool LayerTreeResourceProvider::InUseByConsumer(viz::ResourceId id) {
  auto it = imported_resources_.find(id);
  if (it != imported_resources_.end()) {
    ImportedResource& imported = it->second;
    return imported.exported_count > 0 || imported.returned_lost;
  }

  viz::internal::Resource* resource = GetResource(id);
  return resource->exported_count > 0 || resource->lost;
}

bool LayerTreeResourceProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  // Imported resources should be tracked in the client where they
  // originated, as this code has only a name to refer to them and
  // is not keeping them alive.

  // Non-imported resources are tracked in the base class.
  return ResourceProvider::OnMemoryDump(args, pmd);
}

}  // namespace cc

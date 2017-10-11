// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/layer_tree_resource_provider.h"

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

using gpu::gles2::GLES2Interface;

namespace cc {

LayerTreeResourceProvider::LayerTreeResourceProvider(
    viz::ContextProvider* compositor_context_provider,
    viz::SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool delegated_sync_points_required,
    const viz::ResourceSettings& resource_settings)
    : ResourceProvider(compositor_context_provider,
                       shared_bitmap_manager,
                       gpu_memory_buffer_manager,
                       delegated_sync_points_required,
                       resource_settings) {}

LayerTreeResourceProvider::~LayerTreeResourceProvider() {}

viz::ResourceId LayerTreeResourceProvider::CreateResourceFromTextureMailbox(
    const viz::TextureMailbox& mailbox,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback,
    bool read_lock_fences_enabled,
    gfx::BufferFormat buffer_format) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Just store the information. Mailbox will be consumed in
  // DisplayResourceProvider::LockForRead().
  viz::ResourceId id = next_id_++;
  DCHECK(mailbox.IsValid());
  Resource* resource = nullptr;
  if (mailbox.IsTexture()) {
    resource = InsertResource(
        id, Resource(0, mailbox.size_in_pixels(), Resource::EXTERNAL,
                     mailbox.target(),
                     mailbox.nearest_neighbor() ? GL_NEAREST : GL_LINEAR,
                     TEXTURE_HINT_DEFAULT, RESOURCE_TYPE_GL_TEXTURE,
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
  resource->SetMailbox(mailbox);
  resource->color_space = mailbox.color_space();
  resource->release_callback =
      base::Bind(&viz::SingleReleaseCallback::Run,
                 base::Owned(release_callback.release()));
  resource->read_lock_fences_enabled = read_lock_fences_enabled;
  resource->buffer_format = buffer_format;
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

viz::ResourceId LayerTreeResourceProvider::CreateResourceFromTextureMailbox(
    const viz::TextureMailbox& mailbox,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback,
    bool read_lock_fences_enabled) {
  return CreateResourceFromTextureMailbox(mailbox, std::move(release_callback),
                                          read_lock_fences_enabled,
                                          gfx::BufferFormat::RGBA_8888);
}

viz::ResourceId LayerTreeResourceProvider::CreateResourceFromTextureMailbox(
    const viz::TextureMailbox& mailbox,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  return CreateResourceFromTextureMailbox(mailbox, std::move(release_callback),
                                          false);
}

gpu::SyncToken LayerTreeResourceProvider::GetSyncTokenForResources(
    const ResourceIdArray& resource_ids) {
  gpu::SyncToken latest_sync_token;
  for (viz::ResourceId id : resource_ids) {
    const gpu::SyncToken& sync_token = GetResource(id)->mailbox().sync_token();
    if (sync_token.release_count() > latest_sync_token.release_count())
      latest_sync_token = sync_token;
  }
  return latest_sync_token;
}

void LayerTreeResourceProvider::PrepareSendToParent(
    const ResourceIdArray& resource_ids,
    std::vector<viz::TransferableResource>* list) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GLES2Interface* gl = ContextGL();

  // This function goes through the array multiple times, store the resources
  // as pointers so we don't have to look up the resource id multiple times.
  std::vector<Resource*> resources;
  resources.reserve(resource_ids.size());
  for (const viz::ResourceId id : resource_ids)
    resources.push_back(GetResource(id));

  // Lazily create any mailboxes and verify all unverified sync tokens.
  std::vector<GLbyte*> unverified_sync_tokens;
  std::vector<Resource*> need_synchronization_resources;
  for (Resource* resource : resources) {
    if (!ResourceProvider::IsGpuResourceType(resource->type))
      continue;

    CreateMailbox(resource);

    if (settings_.delegated_sync_points_required) {
      if (resource->needs_sync_token()) {
        need_synchronization_resources.push_back(resource);
      } else if (!resource->mailbox().sync_token().verified_flush()) {
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

  if (compositor_context_provider_)
    compositor_context_provider_->ContextSupport()->FlushPendingWork();

  if (!unverified_sync_tokens.empty()) {
    DCHECK(settings_.delegated_sync_points_required);
    DCHECK(gl);
    gl->VerifySyncTokensCHROMIUM(unverified_sync_tokens.data(),
                                 unverified_sync_tokens.size());
  }

  // Set sync token after verification.
  for (Resource* resource : need_synchronization_resources) {
    DCHECK(ResourceProvider::IsGpuResourceType(resource->type));
    resource->UpdateSyncToken(new_sync_token);
    resource->SetSynchronized();
  }

  // Transfer Resources
  DCHECK_EQ(resources.size(), resource_ids.size());
  for (size_t i = 0; i < resources.size(); ++i) {
    Resource* source = resources[i];
    const viz::ResourceId id = resource_ids[i];

    DCHECK(!settings_.delegated_sync_points_required ||
           !source->needs_sync_token());
    DCHECK(!settings_.delegated_sync_points_required ||
           Resource::LOCALLY_USED != source->synchronization_state());

    viz::TransferableResource resource;
    TransferResource(source, id, &resource);

    source->exported_count++;
    list->push_back(resource);
  }
}

void LayerTreeResourceProvider::ReceiveReturnsFromParent(
    const std::vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GLES2Interface* gl = ContextGL();

  for (const viz::ReturnedResource& returned : resources) {
    viz::ResourceId local_id = returned.id;
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

void LayerTreeResourceProvider::TransferResource(
    Resource* source,
    viz::ResourceId id,
    viz::TransferableResource* resource) {
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
    // This is either an external resource, or a compositor resource that we
    // already exported. Make sure to forward the sync point that we were given.
    resource->mailbox_holder.mailbox = source->mailbox().mailbox();
    resource->mailbox_holder.texture_target = source->mailbox().target();
    resource->mailbox_holder.sync_token = source->mailbox().sync_token();
  }
}

LayerTreeResourceProvider::ScopedWriteLockGpuMemoryBuffer ::
    ScopedWriteLockGpuMemoryBuffer(LayerTreeResourceProvider* resource_provider,
                                   viz::ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  Resource* resource = resource_provider->LockForWrite(resource_id);
  DCHECK(IsGpuResourceType(resource->type));
  size_ = resource->size;
  format_ = resource->format;
  usage_ = resource->usage;
  color_space_ = resource_provider->GetResourceColorSpaceForRaster(resource);
  gpu_memory_buffer_ = std::move(resource->gpu_memory_buffer);
}

LayerTreeResourceProvider::ScopedWriteLockGpuMemoryBuffer::
    ~ScopedWriteLockGpuMemoryBuffer() {
  Resource* resource = resource_provider_->GetResource(resource_id_);
  // Avoid crashing in release builds if GpuMemoryBuffer allocation fails.
  // http://crbug.com/554541
  if (gpu_memory_buffer_) {
    resource->gpu_memory_buffer = std::move(gpu_memory_buffer_);
    resource->allocated = true;
    resource->is_overlay_candidate = true;
    resource->buffer_format = resource->gpu_memory_buffer->GetFormat();
    // GpuMemoryBuffer provides direct access to the memory used by the GPU.
    // Read lock fences are required to ensure that we're not trying to map a
    // buffer that is currently in-use by the GPU.
    resource->read_lock_fences_enabled = true;
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
      gpu_memory_buffer_->SetColorSpaceForScanout(color_space_);
  }
  return gpu_memory_buffer_.get();
}

}  // namespace cc

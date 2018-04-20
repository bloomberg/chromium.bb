// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/layer_tree_resource_provider.h"

#include "base/bits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
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
      : resource(resource),
        release_callback(std::move(release_callback)),
        // If the resource is immediately deleted, it returns the same SyncToken
        // it came with. The client may need to wait on that before deleting the
        // backing or reusing it.
        returned_sync_token(resource.mailbox_holder.sync_token) {
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
}

LayerTreeResourceProvider::~LayerTreeResourceProvider() {
  for (auto& pair : imported_resources_) {
    ImportedResource& imported = pair.second;
    // If the resource is exported we can't report when it can be used again
    // once this class is destroyed, so consider the resource lost.
    bool is_lost = imported.exported_count || imported.returned_lost;
    imported.release_callback->Run(imported.returned_sync_token, is_lost);
  }
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
  if (settings_.delegated_sync_points_required) {
    for (auto& pair : resources) {
      viz::internal::Resource* resource = pair.first;
      if (!resource->is_gpu_resource_type())
        continue;

      if (resource->needs_sync_token()) {
        need_synchronization_resources.push_back(resource);
      } else if (!resource->sync_token().verified_flush()) {
        unverified_sync_tokens.push_back(resource->GetSyncTokenData());
      }
    }
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
    viz::ResourceFormat format,
    const gfx::ColorSpace& color_space) {
  DCHECK(compositor_context_provider_);
  DCHECK(!size.IsEmpty());
  DCHECK_LE(size.width(), settings_.max_texture_size);
  DCHECK_LE(size.height(), settings_.max_texture_size);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsTextureFormatSupported(format));
  DCHECK_NE(format, viz::ETC1);

  viz::ResourceId id = next_id_++;
  viz::internal::Resource* resource =
      InsertResource(id, viz::internal::Resource(
                             size, viz::internal::Resource::INTERNAL,
                             viz::ResourceType::kTexture, format, color_space));
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);

  // Create and set texture properties. Allocation of the texture backing is
  // delayed until needed.
  gl->GenTextures(1, &resource->gl_id);
  gl->BindTexture(resource->target, resource->gl_id);
  gl->TexParameteri(resource->target, GL_TEXTURE_MIN_FILTER,
                    resource->original_filter);
  gl->TexParameteri(resource->target, GL_TEXTURE_MAG_FILTER,
                    resource->original_filter);
  gl->TexParameteri(resource->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(resource->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->TexImage2D(resource->target, 0, GLInternalFormat(format), size.width(),
                 size.height(), 0, GLDataFormat(format), GLDataType(format),
                 nullptr);
  gl->GenMailboxCHROMIUM(resource->mailbox.name);
  gl->ProduceTextureDirectCHROMIUM(resource->gl_id, resource->mailbox.name);
  resource->SetLocallyUsed();
  return id;
}

void LayerTreeResourceProvider::DeleteResource(viz::ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  viz::internal::Resource* resource = &it->second;
  DCHECK(!resource->marked_for_deletion);
  DCHECK_EQ(resource->imported_count, 0);

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
  DCHECK_EQ(resource->origin, viz::internal::Resource::INTERNAL);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->lost);
  DCHECK(!resource->ShouldWaitSyncToken());
  DCHECK_EQ(image_size.width(), resource->size.width());
  DCHECK_EQ(image_size.height(), resource->size.height());

  DCHECK_NE(resource->type, viz::ResourceType::kBitmap);
  GLuint texture_id = resource->gl_id;
  DCHECK(texture_id);
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  gl->BindTexture(resource->target, texture_id);
  gl->TexSubImage2D(resource->target, 0, 0, 0, image_size.width(),
                    image_size.height(), GLDataFormat(resource->format),
                    GLDataType(resource->format), image);
  resource->SetLocallyUsed();
}

void LayerTreeResourceProvider::TransferResource(
    viz::internal::Resource* source,
    viz::ResourceId id,
    viz::TransferableResource* resource) {
  resource->id = id;
  resource->format = source->format;
  resource->buffer_format = source->buffer_format;
  resource->filter = source->filter;
  resource->size = source->size;
  resource->read_lock_fences_enabled = source->read_lock_fences_enabled;
  resource->is_overlay_candidate = false;
  resource->color_space = source->color_space;

  DCHECK_NE(source->type, viz::ResourceType::kBitmap);
  DCHECK(!source->mailbox.IsZero());
  // This is either an external resource, or a compositor resource that we
  // already exported. Make sure to forward the sync point that we were given.
  resource->mailbox_holder.mailbox = source->mailbox;
  resource->mailbox_holder.texture_target = source->target;
  resource->mailbox_holder.sync_token = source->sync_token();
}

void LayerTreeResourceProvider::FlushPendingDeletions() const {
  if (auto* gl = ContextGL())
    gl->ShallowFlushCHROMIUM();
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

viz::ResourceFormat LayerTreeResourceProvider::YuvResourceFormat(
    int bits) const {
  if (bits > 8) {
    return settings_.yuv_highbit_resource_format;
  } else {
    return settings_.yuv_resource_format;
  }
}

void LayerTreeResourceProvider::LoseResourceForTesting(viz::ResourceId id) {
  auto it = imported_resources_.find(id);
  if (it != imported_resources_.end()) {
    ImportedResource& imported = it->second;
    imported.returned_lost = true;
    return;
  }

  viz::internal::Resource* resource = GetResource(id);
  DCHECK(resource);
  resource->lost = true;
}

void LayerTreeResourceProvider::EnableReadLockFencesForTesting(
    viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  DCHECK(resource);
  resource->read_lock_fences_enabled = true;
}

LayerTreeResourceProvider::ScopedSkSurface::ScopedSkSurface(
    GrContext* gr_context,
    GLuint texture_id,
    GLenum texture_target,
    const gfx::Size& size,
    viz::ResourceFormat format,
    bool can_use_lcd_text,
    int msaa_sample_count) {
  GrGLTextureInfo texture_info;
  texture_info.fID = texture_id;
  texture_info.fTarget = texture_target;
  texture_info.fFormat = TextureStorageFormat(format);
  GrBackendTexture backend_texture(size.width(), size.height(),
                                   GrMipMapped::kNo, texture_info);
  SkSurfaceProps surface_props = ComputeSurfaceProps(can_use_lcd_text);
  surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
      gr_context, backend_texture, kTopLeft_GrSurfaceOrigin, msaa_sample_count,
      ResourceFormatToClosestSkColorType(format), nullptr, &surface_props);
}

LayerTreeResourceProvider::ScopedSkSurface::~ScopedSkSurface() {
  if (surface_)
    surface_->prepareForExternalIO();
}

SkSurfaceProps LayerTreeResourceProvider::ScopedSkSurface::ComputeSurfaceProps(
    bool can_use_lcd_text) {
  uint32_t flags = 0;
  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
  if (can_use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props =
        SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  }
  return surface_props;
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

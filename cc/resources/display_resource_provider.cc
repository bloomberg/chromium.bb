// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/display_resource_provider.h"

#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"

using gpu::gles2::GLES2Interface;

namespace cc {

static GLint GetActiveTextureUnit(GLES2Interface* gl) {
  GLint active_unit = 0;
  gl->GetIntegerv(GL_ACTIVE_TEXTURE, &active_unit);
  return active_unit;
}

class ScopedSetActiveTexture {
 public:
  ScopedSetActiveTexture(GLES2Interface* gl, GLenum unit)
      : gl_(gl), unit_(unit) {
    DCHECK_EQ(GL_TEXTURE0, GetActiveTextureUnit(gl_));

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

namespace {
// The resource id in DisplayResourceProvider starts from 2 to avoid
// conflicts with id from LayerTreeResourceProvider.
const unsigned int kDisplayInitialResourceId = 2;
}  // namespace

DisplayResourceProvider::DisplayResourceProvider(
    viz::ContextProvider* compositor_context_provider,
    viz::SharedBitmapManager* shared_bitmap_manager)
    : ResourceProvider(compositor_context_provider),
      next_id_(kDisplayInitialResourceId),
      shared_bitmap_manager_(shared_bitmap_manager) {}

DisplayResourceProvider::~DisplayResourceProvider() {
  while (!children_.empty())
    DestroyChildInternal(children_.begin(), FOR_SHUTDOWN);

  GLES2Interface* gl = ContextGL();
  if (gl)
    gl->Finish();
}

#if defined(OS_ANDROID)
void DisplayResourceProvider::SendPromotionHints(
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

    const viz::internal::Resource* resource = LockForRead(id);
    DCHECK(resource->wants_promotion_hint);

    // Insist that this is backed by a GPU texture.
    if (resource->is_gpu_resource_type()) {
      DCHECK(resource->gl_id);
      auto iter = promotion_hints.find(id);
      bool promotable = iter != promotion_hints.end();
      gl->OverlayPromotionHintCHROMIUM(resource->gl_id, promotable,
                                       promotable ? iter->second.x() : 0,
                                       promotable ? iter->second.y() : 0,
                                       promotable ? iter->second.width() : 0,
                                       promotable ? iter->second.height() : 0);
    }
    UnlockForRead(id);
  }
}

void DisplayResourceProvider::DeletePromotionHint(ResourceMap::iterator it,
                                                  DeleteStyle style) {
  viz::internal::Resource* resource = &it->second;
  // If this resource was interested in promotion hints, then remove it from
  // the set of resources that we'll notify.
  if (resource->wants_promotion_hint)
    wants_promotion_hints_set_.erase(it->first);
}

bool DisplayResourceProvider::IsBackedBySurfaceTexture(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->is_backed_by_surface_texture;
}

bool DisplayResourceProvider::WantsPromotionHintForTesting(viz::ResourceId id) {
  return wants_promotion_hints_set_.count(id) > 0;
}

size_t DisplayResourceProvider::CountPromotionHintRequestsForTesting() {
  return wants_promotion_hints_set_.size();
}

#endif
bool DisplayResourceProvider::IsOverlayCandidate(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->is_overlay_candidate;
}

viz::ResourceType DisplayResourceProvider::GetResourceType(viz::ResourceId id) {
  return GetResource(id)->type;
}

gfx::BufferFormat DisplayResourceProvider::GetBufferFormat(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->buffer_format;
}

void DisplayResourceProvider::WaitSyncToken(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  WaitSyncTokenInternal(resource);
#if defined(OS_ANDROID)
  // Now that the resource is synced, we may send it a promotion hint.  We could
  // sync all |wants_promotion_hint| resources elsewhere, and send 'no' to all
  // resources that weren't used.  However, there's no real advantage.
  if (resource->wants_promotion_hint)
    wants_promotion_hints_set_.insert(id);
#endif  // OS_ANDROID
}

DisplayResourceProvider::ScopedBatchReturnResources::ScopedBatchReturnResources(
    DisplayResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
  resource_provider_->SetBatchReturnResources(true);
}

DisplayResourceProvider::ScopedBatchReturnResources::
    ~ScopedBatchReturnResources() {
  resource_provider_->SetBatchReturnResources(false);
}

void DisplayResourceProvider::SetBatchReturnResources(bool batch) {
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

DisplayResourceProvider::Child::Child()
    : marked_for_deletion(false), needs_sync_tokens(true) {}

DisplayResourceProvider::Child::Child(const Child& other) = default;

DisplayResourceProvider::Child::~Child() = default;

int DisplayResourceProvider::CreateChild(
    const ReturnCallback& return_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Child child_info;
  child_info.return_callback = return_callback;

  int child = next_child_++;
  children_[child] = child_info;
  return child;
}

void DisplayResourceProvider::SetChildNeedsSyncTokens(int child_id,
                                                      bool needs) {
  ChildMap::iterator it = children_.find(child_id);
  DCHECK(it != children_.end());
  it->second.needs_sync_tokens = needs;
}

void DisplayResourceProvider::DestroyChild(int child_id) {
  ChildMap::iterator it = children_.find(child_id);
  DCHECK(it != children_.end());
  DestroyChildInternal(it, NORMAL);
}

void DisplayResourceProvider::DestroyChildInternal(ChildMap::iterator it,
                                                   DeleteStyle style) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Child& child = it->second;
  DCHECK(style == FOR_SHUTDOWN || !child.marked_for_deletion);

  ResourceIdArray resources_for_child;

  for (ResourceIdMap::iterator child_it = child.child_to_parent_map.begin();
       child_it != child.child_to_parent_map.end(); ++child_it) {
    viz::ResourceId id = child_it->second;
    resources_for_child.push_back(id);
  }

  child.marked_for_deletion = true;

  DeleteAndReturnUnusedResourcesToChild(it, style, resources_for_child);
}

void DisplayResourceProvider::DeleteAndReturnUnusedResourcesToChild(
    ChildMap::iterator child_it,
    DeleteStyle style,
    const ResourceIdArray& unused) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(child_it != children_.end());
  Child* child_info = &child_it->second;

  if (unused.empty() && !child_info->marked_for_deletion)
    return;

  std::vector<viz::ReturnedResource> to_return;
  to_return.reserve(unused.size());
  std::vector<viz::ReturnedResource*> need_synchronization_resources;
  std::vector<GLbyte*> unverified_sync_tokens;
  std::vector<size_t> to_return_indices_unverified;

  GLES2Interface* gl = ContextGL();

  for (viz::ResourceId local_id : unused) {
    ResourceMap::iterator it = resources_.find(local_id);
    CHECK(it != resources_.end());
    viz::internal::Resource& resource = it->second;

    // TODO(xing.xu): remove locked_for_write.
    DCHECK(!resource.locked_for_write);

    viz::ResourceId child_id = resource.id_in_child;
    DCHECK(child_info->child_to_parent_map.count(child_id));

    bool is_lost = resource.lost ||
                   (resource.is_gpu_resource_type() && lost_context_provider_);
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

    if (resource.is_gpu_resource_type() &&
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

    viz::ReturnedResource returned;
    returned.id = child_id;
    returned.sync_token = resource.sync_token();
    returned.count = resource.imported_count;
    returned.lost = is_lost;
    to_return.push_back(returned);

    if (resource.is_gpu_resource_type() && child_info->needs_sync_tokens) {
      if (resource.needs_sync_token()) {
        need_synchronization_resources.push_back(&to_return.back());
      } else if (returned.sync_token.HasData() &&
                 !returned.sync_token.verified_flush()) {
        // Before returning any sync tokens, they must be verified. Store an
        // index into |to_return| instead of a pointer as vectors may realloc
        // and move their data.
        to_return_indices_unverified.push_back(to_return.size() - 1);
      }
    }

    child_info->child_to_parent_map.erase(child_id);
    resource.imported_count = 0;
#if defined(OS_ANDROID)
    DeletePromotionHint(it, style);
#endif
    DeleteResourceInternal(it, style);
  }

  for (size_t i : to_return_indices_unverified)
    unverified_sync_tokens.push_back(to_return[i].sync_token.GetData());

  gpu::SyncToken new_sync_token;
  if (!need_synchronization_resources.empty()) {
    DCHECK(child_info->needs_sync_tokens);
    DCHECK(gl);
    gl->GenUnverifiedSyncTokenCHROMIUM(new_sync_token.GetData());
    unverified_sync_tokens.push_back(new_sync_token.GetData());
  }

  if (!unverified_sync_tokens.empty()) {
    DCHECK(child_info->needs_sync_tokens);
    DCHECK(gl);
    gl->VerifySyncTokensCHROMIUM(unverified_sync_tokens.data(),
                                 unverified_sync_tokens.size());
  }

  // Set sync token after verification.
  for (viz::ReturnedResource* returned : need_synchronization_resources)
    returned->sync_token = new_sync_token;

  if (!to_return.empty())
    child_info->return_callback.Run(to_return);

  if (child_info->marked_for_deletion &&
      child_info->child_to_parent_map.empty()) {
    children_.erase(child_it);
  }
}

void DisplayResourceProvider::ReceiveFromChild(
    int child,
    const std::vector<viz::TransferableResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GLES2Interface* gl = ContextGL();
  Child& child_info = children_.find(child)->second;
  DCHECK(!child_info.marked_for_deletion);
  for (std::vector<viz::TransferableResource>::const_iterator it =
           resources.begin();
       it != resources.end(); ++it) {
    ResourceIdMap::iterator resource_in_map_it =
        child_info.child_to_parent_map.find(it->id);
    if (resource_in_map_it != child_info.child_to_parent_map.end()) {
      viz::internal::Resource* resource =
          GetResource(resource_in_map_it->second);
      resource->marked_for_deletion = false;
      resource->imported_count++;
      continue;
    }

    if ((!it->is_software && !gl) ||
        (it->is_software && !shared_bitmap_manager_)) {
      TRACE_EVENT0(
          "cc", "DisplayResourceProvider::ReceiveFromChild dropping invalid");
      std::vector<viz::ReturnedResource> to_return;
      to_return.push_back(it->ToReturnedResource());
      child_info.return_callback.Run(to_return);
      continue;
    }

    viz::ResourceId local_id = next_id_++;
    viz::internal::Resource* resource = nullptr;
    if (it->is_software) {
      DCHECK(IsBitmapFormatSupported(it->format));
      resource = InsertResource(
          local_id,
          viz::internal::Resource(it->size, viz::internal::Resource::DELEGATED,
                                  viz::ResourceTextureHint::kDefault,
                                  viz::ResourceType::kBitmap, it->format,
                                  it->color_space));
      resource->has_shared_bitmap_id = true;
      resource->shared_bitmap_id = it->mailbox_holder.mailbox;
    } else {
      resource = InsertResource(
          local_id,
          viz::internal::Resource(it->size, viz::internal::Resource::DELEGATED,
                                  viz::ResourceTextureHint::kDefault,
                                  viz::ResourceType::kTexture, it->format,
                                  it->color_space));
      resource->target = it->mailbox_holder.texture_target;
      resource->filter = it->filter;
      resource->original_filter = it->filter;
      resource->min_filter = it->filter;
      resource->buffer_format = it->buffer_format;
      resource->mailbox = it->mailbox_holder.mailbox;
      resource->UpdateSyncToken(it->mailbox_holder.sync_token);
      resource->read_lock_fences_enabled = it->read_lock_fences_enabled;
      resource->is_overlay_candidate = it->is_overlay_candidate;
#if defined(OS_ANDROID)
      resource->is_backed_by_surface_texture = it->is_backed_by_surface_texture;
      resource->wants_promotion_hint = it->wants_promotion_hint;
#endif
    }
    resource->child_id = child;
    // Don't allocate a texture for a child.
    resource->allocated = true;
    resource->imported_count = 1;
    resource->id_in_child = it->id;
    child_info.child_to_parent_map[it->id] = local_id;
  }
}

void DisplayResourceProvider::DeclareUsedResourcesFromChild(
    int child,
    const viz::ResourceIdSet& resources_from_child) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ChildMap::iterator child_it = children_.find(child);
  DCHECK(child_it != children_.end());
  Child& child_info = child_it->second;
  DCHECK(!child_info.marked_for_deletion);

  ResourceIdArray unused;
  for (ResourceIdMap::iterator it = child_info.child_to_parent_map.begin();
       it != child_info.child_to_parent_map.end(); ++it) {
    viz::ResourceId local_id = it->second;
    bool resource_is_in_use = resources_from_child.count(it->first) > 0;
    if (!resource_is_in_use)
      unused.push_back(local_id);
  }
  DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, unused);
}

const ResourceProvider::ResourceIdMap&
DisplayResourceProvider::GetChildToParentMap(int child) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ChildMap::const_iterator it = children_.find(child);
  DCHECK(it != children_.end());
  DCHECK(!it->second.marked_for_deletion);
  return it->second.child_to_parent_map;
}

GLenum DisplayResourceProvider::BindForSampling(viz::ResourceId resource_id,
                                                GLenum unit,
                                                GLenum filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GLES2Interface* gl = ContextGL();
  ResourceMap::iterator it = resources_.find(resource_id);
  DCHECK(it != resources_.end());
  viz::internal::Resource* resource = &it->second;
  DCHECK(resource->lock_for_read_count);
  // TODO(xing.xu): remove locked_for_write.
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
        DCHECK(compositor_context_provider_);
        DCHECK(
            compositor_context_provider_->ContextCapabilities().texture_npot);
        gl->GenerateMipmap(target);
        resource->mipmap_state = viz::internal::Resource::VALID;
        FALLTHROUGH;
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

bool DisplayResourceProvider::InUse(viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  return resource->lock_for_read_count > 0 || resource->lost;
}

viz::ResourceMetadata
DisplayResourceProvider::GetResourceMetadataForExternalUse(
    viz::ResourceId id,
    const gpu::SyncToken& release_sync_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ResourceMap::iterator it = resources_.find(id);
  DCHECK(it != resources_.end());

  viz::internal::Resource* resource = &it->second;
  viz::ResourceMetadata metadata;
  // TODO(xing.xu): remove locked_for_write.
  DCHECK(!resource->locked_for_write)
      << "locked for write: " << resource->locked_for_write;
  DCHECK_EQ(resource->exported_count, 0);
  // Uninitialized! Call SetPixels or LockForWrite first.
  DCHECK(resource->allocated);
  // TODO(penghuang): support software resource.
  DCHECK(resource->is_gpu_resource_type());

  metadata.mailbox = resource->mailbox;
  metadata.backend_format = GrBackendFormat::MakeGL(
      TextureStorageFormat(resource->format), resource->target);
  metadata.size = resource->size;
  metadata.mip_mapped = GrMipMapped::kNo;
  metadata.origin = kTopLeft_GrSurfaceOrigin;
  metadata.color_type = ResourceFormatToClosestSkColorType(resource->format);
  metadata.alpha_type = kPremul_SkAlphaType;
  metadata.color_space = nullptr;
  metadata.sync_token = resource->sync_token();

  // Update the resource sync token to |release_sync_token|. When the next frame
  // is being composited, the DeclareUsedResourcesFromChild() will be called
  // with resources belong to every child for the next frame. If the resource
  // is not used by the next frame, the resource will be returned to a child
  // which owns it with the |release_sync_token|. The child is responsible for
  // issuing a WaitSyncToken GL command with the |release_sync_token| before
  // reusing it.
  resource->UpdateSyncToken(release_sync_token);

  return metadata;
}

DisplayResourceProvider::ScopedReadLockGL::ScopedReadLockGL(
    DisplayResourceProvider* resource_provider,
    viz::ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const viz::internal::Resource* resource =
      resource_provider->LockForRead(resource_id);
  texture_id_ = resource->gl_id;
  target_ = resource->target;
  size_ = resource->size;
  color_space_ = resource->color_space;
}

const viz::internal::Resource* DisplayResourceProvider::LockForRead(
    viz::ResourceId id) {
  viz::internal::Resource* resource = GetResource(id);
  // TODO(xing.xu): remove locked_for_write.
  DCHECK(!resource->locked_for_write)
      << "locked for write: " << resource->locked_for_write;
  DCHECK_EQ(resource->exported_count, 0);
  // Uninitialized! Call SetPixels or LockForWrite first.
  DCHECK(resource->allocated);

  // Mailbox sync_tokens must be processed by a call to WaitSyncToken() prior to
  // calling LockForRead().
  DCHECK_NE(viz::internal::Resource::NEEDS_WAIT,
            resource->synchronization_state());

  if (resource->is_gpu_resource_type() && !resource->gl_id) {
    DCHECK(resource->origin != viz::internal::Resource::INTERNAL);
    DCHECK(!resource->mailbox.IsZero());

    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    resource->gl_id =
        gl->CreateAndConsumeTextureCHROMIUM(resource->mailbox.name);
    resource->SetLocallyUsed();
  }

  if (!resource->pixels && resource->has_shared_bitmap_id &&
      shared_bitmap_manager_) {
    std::unique_ptr<viz::SharedBitmap> bitmap =
        shared_bitmap_manager_->GetSharedBitmapFromId(
            resource->size, resource->format, resource->shared_bitmap_id);
    if (bitmap) {
      resource->SetSharedBitmap(bitmap.get());
      resource->owned_shared_bitmap = std::move(bitmap);
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

void DisplayResourceProvider::UnlockForRead(viz::ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());

  viz::internal::Resource* resource = &it->second;
  DCHECK_GT(resource->lock_for_read_count, 0);
  DCHECK_EQ(resource->exported_count, 0);
  resource->lock_for_read_count--;
  if (resource->marked_for_deletion && !resource->lock_for_read_count) {
    if (!resource->child_id) {
      // The resource belongs to this ResourceProvider, so it can be destroyed.
#if defined(OS_ANDROID)
      DeletePromotionHint(it, NORMAL);
#endif
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

bool DisplayResourceProvider::ReadLockFenceHasPassed(
    const viz::internal::Resource* resource) {
  return !resource->read_lock_fence || resource->read_lock_fence->HasPassed();
}

DisplayResourceProvider::ScopedReadLockGL::~ScopedReadLockGL() {
  resource_provider_->UnlockForRead(resource_id_);
}

DisplayResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    DisplayResourceProvider* resource_provider,
    viz::ResourceId resource_id,
    GLenum filter)
    : resource_lock_(resource_provider, resource_id),
      unit_(GL_TEXTURE0),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {}

DisplayResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    DisplayResourceProvider* resource_provider,
    viz::ResourceId resource_id,
    GLenum unit,
    GLenum filter)
    : resource_lock_(resource_provider, resource_id),
      unit_(unit),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {}

DisplayResourceProvider::ScopedSamplerGL::~ScopedSamplerGL() = default;

DisplayResourceProvider::ScopedReadLockSkImage::ScopedReadLockSkImage(
    DisplayResourceProvider* resource_provider,
    viz::ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const viz::internal::Resource* resource =
      resource_provider->LockForRead(resource_id);
  if (resource_provider_->resource_sk_image_.find(resource_id) !=
      resource_provider_->resource_sk_image_.end()) {
    // Use cached sk_image.
    sk_image_ =
        resource_provider_->resource_sk_image_.find(resource_id)->second;
  } else if (resource->gl_id) {
    GrGLTextureInfo texture_info;
    texture_info.fID = resource->gl_id;
    texture_info.fTarget = resource->target;
    texture_info.fFormat = TextureStorageFormat(resource->format);
    GrBackendTexture backend_texture(resource->size.width(),
                                     resource->size.height(), GrMipMapped::kNo,
                                     texture_info);
    sk_image_ = SkImage::MakeFromTexture(
        resource_provider->compositor_context_provider_->GrContext(),
        backend_texture, kTopLeft_GrSurfaceOrigin,
        ResourceFormatToClosestSkColorType(resource->format),
        kPremul_SkAlphaType, nullptr);
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

DisplayResourceProvider::ScopedReadLockSkImage::~ScopedReadLockSkImage() {
  resource_provider_->UnlockForRead(resource_id_);
}

DisplayResourceProvider::ScopedReadLockSoftware::ScopedReadLockSoftware(
    DisplayResourceProvider* resource_provider,
    viz::ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const viz::internal::Resource* resource =
      resource_provider->LockForRead(resource_id);
  resource_provider->PopulateSkBitmapWithResource(&sk_bitmap_, resource);
}

DisplayResourceProvider::ScopedReadLockSoftware::~ScopedReadLockSoftware() {
  resource_provider_->UnlockForRead(resource_id_);
}

DisplayResourceProvider::SynchronousFence::SynchronousFence(
    gpu::gles2::GLES2Interface* gl)
    : gl_(gl), has_synchronized_(true) {}

DisplayResourceProvider::SynchronousFence::~SynchronousFence() = default;

void DisplayResourceProvider::SynchronousFence::Set() {
  has_synchronized_ = false;
}

bool DisplayResourceProvider::SynchronousFence::HasPassed() {
  if (!has_synchronized_) {
    has_synchronized_ = true;
    Synchronize();
  }
  return true;
}

void DisplayResourceProvider::SynchronousFence::Wait() {
  HasPassed();
}

void DisplayResourceProvider::SynchronousFence::Synchronize() {
  TRACE_EVENT0("cc", "DisplayResourceProvider::SynchronousFence::Synchronize");
  gl_->Finish();
}

}  // namespace cc

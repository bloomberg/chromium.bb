// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_provider.h"

#include <limits.h>

#include "base/debug/alias.h"
#include "base/hash_tables.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "cc/resources/platform_color.h"
#include "cc/resources/transferable_resource.h"
#include "cc/scheduler/texture_uploader.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

using WebKit::WebGraphicsContext3D;

namespace cc {

static GLenum TextureToStorageFormat(GLenum texture_format) {
  GLenum storage_format = GL_RGBA8_OES;
  switch (texture_format) {
    case GL_RGBA:
      break;
    case GL_BGRA_EXT:
      storage_format = GL_BGRA8_EXT;
      break;
    default:
      NOTREACHED();
      break;
  }

  return storage_format;
}

static bool IsTextureFormatSupportedForStorage(GLenum format) {
  return (format == GL_RGBA || format == GL_BGRA_EXT);
}

static unsigned CreateTextureId(WebGraphicsContext3D* context3d) {
  unsigned texture_id = 0;
  GLC(context3d, texture_id = context3d->createTexture());
  GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, texture_id));
  GLC(context3d, context3d->texParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GLC(context3d, context3d->texParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GLC(context3d, context3d->texParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GLC(context3d, context3d->texParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  return texture_id;
}

ResourceProvider::Resource::Resource()
    : gl_id(0),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      pixels(NULL),
      pixel_buffer(NULL),
      lock_for_read_count(0),
      locked_for_write(false),
      external(false),
      exported(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      read_lock_fence(NULL),
      size(),
      format(0),
      filter(0),
      type(static_cast<ResourceType>(0)) {}

ResourceProvider::Resource::~Resource() {}

ResourceProvider::Resource::Resource(
    unsigned texture_id, gfx::Size size, GLenum format, GLenum filter)
    : gl_id(texture_id),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      pixels(NULL),
      pixel_buffer(NULL),
      lock_for_read_count(0),
      locked_for_write(false),
      external(false),
      exported(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      read_lock_fence(NULL),
      size(size),
      format(format),
      filter(filter),
      type(GLTexture) {}

ResourceProvider::Resource::Resource(
    uint8_t* pixels, gfx::Size size, GLenum format, GLenum filter)
    : gl_id(0),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      pixels(pixels),
      pixel_buffer(NULL),
      lock_for_read_count(0),
      locked_for_write(false),
      external(false),
      exported(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      read_lock_fence(NULL),
      size(size),
      format(format),
      filter(filter),
      type(Bitmap) {}

ResourceProvider::Child::Child() {}

ResourceProvider::Child::~Child() {}

scoped_ptr<ResourceProvider> ResourceProvider::Create(
    OutputSurface* output_surface) {
  scoped_ptr<ResourceProvider> resource_provider(
      new ResourceProvider(output_surface));
  if (!resource_provider->Initialize())
    return scoped_ptr<ResourceProvider>();
  return resource_provider.Pass();
}

ResourceProvider::~ResourceProvider() {
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  if (!context3d || !context3d->makeContextCurrent())
    return;
  texture_uploader_.reset();
  texture_copier_.reset();
}

WebGraphicsContext3D* ResourceProvider::GraphicsContext3D() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return output_surface_->context3d();
}

bool ResourceProvider::InUseByConsumer(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  return !!resource->lock_for_read_count || resource->exported;
}

ResourceProvider::ResourceId ResourceProvider::CreateResource(
    gfx::Size size, GLenum format, TextureUsageHint hint) {
  switch (default_resource_type_) {
    case GLTexture:
      return CreateGLTexture(
          size, format, GL_TEXTURE_POOL_UNMANAGED_CHROMIUM, hint);
    case Bitmap:
      DCHECK(format == GL_RGBA);
      return CreateBitmap(size);
  }

  LOG(FATAL) << "Invalid default resource type.";
  return 0;
}

ResourceProvider::ResourceId ResourceProvider::CreateManagedResource(
    gfx::Size size, GLenum format, TextureUsageHint hint) {
  switch (default_resource_type_) {
    case GLTexture:
      return CreateGLTexture(
          size, format, GL_TEXTURE_POOL_MANAGED_CHROMIUM, hint);
    case Bitmap:
      DCHECK(format == GL_RGBA);
      return CreateBitmap(size);
  }

  LOG(FATAL) << "Invalid default resource type.";
  return 0;
}

ResourceProvider::ResourceId ResourceProvider::CreateGLTexture(
    gfx::Size size, GLenum format, GLenum texture_pool, TextureUsageHint hint) {
  DCHECK_LE(size.width(), max_texture_size_);
  DCHECK_LE(size.height(), max_texture_size_);

  DCHECK(thread_checker_.CalledOnValidThread());
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  DCHECK(context3d);

  // Create and set texture properties. Allocation is delayed until needed.
  unsigned texture_id = CreateTextureId(context3d);
  GLC(context3d, context3d->texParameteri(GL_TEXTURE_2D,
                                          GL_TEXTURE_POOL_CHROMIUM,
                                          texture_pool));
  if (use_texture_usage_hint_ && hint == TextureUsageFramebuffer) {
    GLC(context3d, context3d->texParameteri(GL_TEXTURE_2D,
                                            GL_TEXTURE_USAGE_ANGLE,
                                            GL_FRAMEBUFFER_ATTACHMENT_ANGLE));
  }

  ResourceId id = next_id_++;
  Resource resource(texture_id, size, format, GL_LINEAR);
  resource.allocated = false;
  resources_[id] = resource;
  return id;
}

ResourceProvider::ResourceId ResourceProvider::CreateBitmap(gfx::Size size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  uint8_t* pixels = new uint8_t[size.width() * size.height() * 4];

  ResourceId id = next_id_++;
  Resource resource(pixels, size, GL_RGBA, GL_LINEAR);
  resource.allocated = true;
  resources_[id] = resource;
  return id;
}

ResourceProvider::ResourceId ResourceProvider::
    CreateResourceFromExternalTexture(unsigned texture_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  WebGraphicsContext3D* context3d = output_surface_->context3d();
  DCHECK(context3d);
  GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, texture_id));
  GLC(context3d, context3d->texParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GLC(context3d, context3d->texParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GLC(context3d, context3d->texParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GLC(context3d, context3d->texParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

  ResourceId id = next_id_++;
  Resource resource(texture_id, gfx::Size(), 0, GL_LINEAR);
  resource.external = true;
  resource.allocated = true;
  resources_[id] = resource;
  return id;
}

ResourceProvider::ResourceId ResourceProvider::CreateResourceFromTextureMailbox(
    const TextureMailbox& mailbox) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Just store the information. Mailbox will be consumed in LockForRead().
  ResourceId id = next_id_++;
  unsigned texture_id = 0;
  Resource resource(texture_id, gfx::Size(), 0, GL_LINEAR);
  resource.external = true;
  resource.allocated = true;
  resource.mailbox = mailbox;
  resources_[id] = resource;
  return id;
}

void ResourceProvider::DeleteResource(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->lock_for_read_count);
  DCHECK(!resource->marked_for_deletion);
  DCHECK(resource->pending_set_pixels || !resource->locked_for_write);

  if (resource->exported) {
    resource->marked_for_deletion = true;
    return;
  } else {
    DeleteResourceInternal(it);
  }
}

void ResourceProvider::DeleteResourceInternal(ResourceMap::iterator it) {
  Resource* resource = &it->second;
  if (resource->gl_id && !resource->external) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    GLC(context3d, context3d->deleteTexture(resource->gl_id));
  }
  if (resource->gl_upload_query_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    GLC(context3d, context3d->deleteQueryEXT(resource->gl_upload_query_id));
  }
  if (resource->gl_pixel_buffer_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    GLC(context3d, context3d->deleteBuffer(resource->gl_pixel_buffer_id));
  }
  if (!resource->mailbox.IsEmpty() && resource->external) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    unsigned sync_point = resource->mailbox.sync_point();
    if (resource->gl_id) {
      GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, resource->gl_id));
      GLC(context3d, context3d->produceTextureCHROMIUM(
          GL_TEXTURE_2D, resource->mailbox.data()));
      GLC(context3d, context3d->deleteTexture(resource->gl_id));
      sync_point = context3d->insertSyncPoint();
    }
    resource->mailbox.RunReleaseCallback(sync_point);
  }
  if (resource->pixels)
    delete[] resource->pixels;
  if (resource->pixel_buffer)
    delete[] resource->pixel_buffer;

  resources_.erase(it);
}

ResourceProvider::ResourceType ResourceProvider::GetResourceType(
    ResourceId id) {
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  return resource->type;
}

void ResourceProvider::SetPixels(ResourceId id,
                                 const uint8_t* image,
                                 gfx::Rect image_rect,
                                 gfx::Rect source_rect,
                                 gfx::Vector2d dest_offset) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->locked_for_write);
  DCHECK(!resource->lock_for_read_count);
  DCHECK(!resource->external);
  DCHECK(!resource->exported);
  DCHECK(ReadLockFenceHasPassed(resource));
  LazyAllocate(resource);

  if (resource->gl_id) {
    DCHECK(!resource->pending_set_pixels);
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    DCHECK(texture_uploader_.get());
    context3d->bindTexture(GL_TEXTURE_2D, resource->gl_id);
    texture_uploader_->Upload(image,
                              image_rect,
                              source_rect,
                              dest_offset,
                              resource->format,
                              resource->size);
  }

  if (resource->pixels) {
    DCHECK(resource->allocated);
    DCHECK(resource->format == GL_RGBA);
    SkBitmap src_full;
    src_full.setConfig(
        SkBitmap::kARGB_8888_Config, image_rect.width(), image_rect.height());
    src_full.setPixels(const_cast<uint8_t*>(image));
    SkBitmap src_subset;
    SkIRect sk_source_rect = SkIRect::MakeXYWH(source_rect.x(),
                                               source_rect.y(),
                                               source_rect.width(),
                                               source_rect.height());
    sk_source_rect.offset(-image_rect.x(), -image_rect.y());
    src_full.extractSubset(&src_subset, sk_source_rect);

    ScopedWriteLockSoftware lock(this, id);
    SkCanvas* dest = lock.sk_canvas();
    dest->writePixels(src_subset, dest_offset.x(), dest_offset.y());
  }
}

size_t ResourceProvider::NumBlockingUploads() {
  if (!texture_uploader_)
    return 0;

  return texture_uploader_->NumBlockingUploads();
}

void ResourceProvider::MarkPendingUploadsAsNonBlocking() {
  if (!texture_uploader_)
    return;

  texture_uploader_->MarkPendingUploadsAsNonBlocking();
}

double ResourceProvider::EstimatedUploadsPerSecond() {
  if (!texture_uploader_)
    return 0.0;

  return texture_uploader_->EstimatedTexturesPerSecond();
}

void ResourceProvider::FlushUploads() {
  if (!texture_uploader_)
    return;

  texture_uploader_->Flush();
}

void ResourceProvider::ReleaseCachedData() {
  if (!texture_uploader_)
    return;

  texture_uploader_->ReleaseCachedQueries();
}

void ResourceProvider::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  if (context3d)
    context3d->flush();
}

bool ResourceProvider::ShallowFlushIfSupported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  if (!context3d || !use_shallow_flush_)
    return false;

  context3d->shallowFlushCHROMIUM();
  return true;
}

const ResourceProvider::Resource* ResourceProvider::LockForRead(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->locked_for_write || resource->set_pixels_completion_forced);
  DCHECK(!resource->exported);
  // Uninitialized! Call SetPixels or LockForWrite first.
  DCHECK(resource->allocated);

  if (!resource->gl_id && resource->external && !resource->mailbox.IsEmpty()) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    if (resource->mailbox.sync_point()) {
      GLC(context3d, context3d->waitSyncPoint(resource->mailbox.sync_point()));
      resource->mailbox.ResetSyncPoint();
    }
    resource->gl_id = context3d->createTexture();
    GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, resource->gl_id));
    GLC(context3d, context3d->consumeTextureCHROMIUM(GL_TEXTURE_2D,
                                                     resource->mailbox.data()));
  }

  resource->lock_for_read_count++;
  if (resource->enable_read_lock_fences)
    resource->read_lock_fence = current_read_lock_fence_;

  return resource;
}

void ResourceProvider::UnlockForRead(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK_GT(resource->lock_for_read_count, 0);
  DCHECK(!resource->exported);
  resource->lock_for_read_count--;
}

const ResourceProvider::Resource* ResourceProvider::LockForWrite(
    ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->locked_for_write);
  DCHECK(!resource->lock_for_read_count);
  DCHECK(!resource->exported);
  DCHECK(!resource->external);
  DCHECK(ReadLockFenceHasPassed(resource));
  LazyAllocate(resource);

  resource->locked_for_write = true;
  return resource;
}

bool ResourceProvider::CanLockForWrite(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  return !resource->locked_for_write &&
      !resource->lock_for_read_count &&
      !resource->exported &&
      !resource->external &&
      ReadLockFenceHasPassed(resource);
}

void ResourceProvider::UnlockForWrite(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(resource->locked_for_write);
  DCHECK(!resource->exported);
  DCHECK(!resource->external);
  resource->locked_for_write = false;
}

ResourceProvider::ScopedReadLockGL::ScopedReadLockGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id),
      texture_id_(resource_provider->LockForRead(resource_id)->gl_id) {
  DCHECK(texture_id_);
}

ResourceProvider::ScopedReadLockGL::~ScopedReadLockGL() {
  resource_provider_->UnlockForRead(resource_id_);
}

ResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id,
    GLenum target,
    GLenum filter)
    : ScopedReadLockGL(resource_provider, resource_id) {
  resource_provider->BindForSampling(resource_id, target, filter);
}

ResourceProvider::ScopedWriteLockGL::ScopedWriteLockGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id),
      texture_id_(resource_provider->LockForWrite(resource_id)->gl_id) {
  DCHECK(texture_id_);
}

ResourceProvider::ScopedWriteLockGL::~ScopedWriteLockGL() {
  resource_provider_->UnlockForWrite(resource_id_);
}

void ResourceProvider::PopulateSkBitmapWithResource(
    SkBitmap* sk_bitmap, const Resource* resource) {
  DCHECK(resource->pixels);
  DCHECK(resource->format == GL_RGBA);
  sk_bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                       resource->size.width(),
                       resource->size.height());
  sk_bitmap->setPixels(resource->pixels);
}

ResourceProvider::ScopedReadLockSoftware::ScopedReadLockSoftware(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id) {
  ResourceProvider::PopulateSkBitmapWithResource(
      &sk_bitmap_, resource_provider->LockForRead(resource_id));
}

ResourceProvider::ScopedReadLockSoftware::~ScopedReadLockSoftware() {
  resource_provider_->UnlockForRead(resource_id_);
}

ResourceProvider::ScopedWriteLockSoftware::ScopedWriteLockSoftware(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id) {
  ResourceProvider::PopulateSkBitmapWithResource(
      &sk_bitmap_, resource_provider->LockForWrite(resource_id));
  sk_canvas_.reset(new SkCanvas(sk_bitmap_));
}

ResourceProvider::ScopedWriteLockSoftware::~ScopedWriteLockSoftware() {
  resource_provider_->UnlockForWrite(resource_id_);
}

ResourceProvider::ResourceProvider(OutputSurface* output_surface)
    : output_surface_(output_surface),
      next_id_(1),
      next_child_(1),
      default_resource_type_(GLTexture),
      use_texture_storage_ext_(false),
      use_texture_usage_hint_(false),
      use_shallow_flush_(false),
      max_texture_size_(0),
      best_texture_format_(0) {}

bool ResourceProvider::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  if (!context3d) {
    max_texture_size_ = INT_MAX / 2;
    best_texture_format_ = GL_RGBA;
    return true;
  }
  if (!context3d->makeContextCurrent())
    return false;

  std::string extensions_string =
      UTF16ToASCII(context3d->getString(GL_EXTENSIONS));
  std::vector<std::string> extensions;
  base::SplitString(extensions_string, ' ', &extensions);
  bool use_map_sub = false;
  bool use_bind_uniform = false;
  bool use_bgra = false;
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (extensions[i] == "GL_EXT_texture_storage")
      use_texture_storage_ext_ = true;
    else if (extensions[i] == "GL_ANGLE_texture_usage")
      use_texture_usage_hint_ = true;
    else if (extensions[i] == "GL_CHROMIUM_map_sub")
      use_map_sub = true;
    else if (extensions[i] == "GL_CHROMIUM_shallow_flush")
      use_shallow_flush_ = true;
    else if (extensions[i] == "GL_CHROMIUM_bind_uniform_location")
      use_bind_uniform = true;
    else if (extensions[i] == "GL_EXT_texture_format_BGRA8888")
      use_bgra = true;
  }

  texture_copier_ =
      AcceleratedTextureCopier::Create(context3d, use_bind_uniform);

  texture_uploader_ =
      TextureUploader::Create(context3d, use_map_sub, use_shallow_flush_);
  GLC(context3d, context3d->getIntegerv(GL_MAX_TEXTURE_SIZE,
                                        &max_texture_size_));
  best_texture_format_ =
      PlatformColor::BestTextureFormat(context3d, use_bgra);
  return true;
}

int ResourceProvider::CreateChild() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Child child_info;
  int child = next_child_++;
  children_[child] = child_info;
  return child;
}

void ResourceProvider::DestroyChild(int child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ChildMap::iterator it = children_.find(child_id);
  DCHECK(it != children_.end());
  Child& child = it->second;
  for (ResourceIdMap::iterator child_it = child.child_to_parent_map.begin();
       child_it != child.child_to_parent_map.end();
       ++child_it)
    DeleteResource(child_it->second);
  children_.erase(it);
}

const ResourceProvider::ResourceIdMap& ResourceProvider::GetChildToParentMap(
    int child) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  ChildMap::const_iterator it = children_.find(child);
  DCHECK(it != children_.end());
  return it->second.child_to_parent_map;
}

void ResourceProvider::PrepareSendToParent(const ResourceIdArray& resources,
                                           TransferableResourceArray* list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  list->clear();
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  if (!context3d || !context3d->makeContextCurrent()) {
    // FIXME: Implement this path for software compositing.
    return;
  }
  bool need_sync_point = false;
  for (ResourceIdArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    TransferableResource resource;
    if (TransferResource(context3d, *it, &resource)) {
      if (!resource.sync_point)
        need_sync_point = true;
      resources_.find(*it)->second.exported = true;
      list->push_back(resource);
    }
  }
  if (need_sync_point) {
    unsigned int sync_point = context3d->insertSyncPoint();
    for (TransferableResourceArray::iterator it = list->begin();
         it != list->end();
         ++it) {
      if (!it->sync_point)
        it->sync_point = sync_point;
    }
  }
}

void ResourceProvider::PrepareSendToChild(int child,
                                          const ResourceIdArray& resources,
                                          TransferableResourceArray* list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  list->clear();
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  if (!context3d || !context3d->makeContextCurrent()) {
    // FIXME: Implement this path for software compositing.
    return;
  }
  Child& child_info = children_.find(child)->second;
  bool need_sync_point = false;
  for (ResourceIdArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    TransferableResource resource;
    if (!TransferResource(context3d, *it, &resource))
      NOTREACHED();
    if (!resource.sync_point)
      need_sync_point = true;
    DCHECK(child_info.parent_to_child_map.find(*it) !=
           child_info.parent_to_child_map.end());
    resource.id = child_info.parent_to_child_map[*it];
    child_info.parent_to_child_map.erase(*it);
    child_info.child_to_parent_map.erase(resource.id);
    list->push_back(resource);
    DeleteResource(*it);
  }
  if (need_sync_point) {
    unsigned int sync_point = context3d->insertSyncPoint();
    for (TransferableResourceArray::iterator it = list->begin();
         it != list->end();
         ++it) {
      if (!it->sync_point)
        it->sync_point = sync_point;
    }
  }
}

void ResourceProvider::ReceiveFromChild(
    int child, const TransferableResourceArray& resources) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  if (!context3d || !context3d->makeContextCurrent()) {
    // FIXME: Implement this path for software compositing.
    return;
  }
  Child& child_info = children_.find(child)->second;
  for (TransferableResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    unsigned texture_id;
    // NOTE: If the parent is a browser and the child a renderer, the parent
    // is not supposed to have its context wait, because that could induce
    // deadlocks and/or security issues. The caller is responsible for
    // waiting asynchronously, and resetting sync_point before calling this.
    // However if the parent is a renderer (e.g. browser tag), it may be ok
    // (and is simpler) to wait.
    if (it->sync_point)
      GLC(context3d, context3d->waitSyncPoint(it->sync_point));
    GLC(context3d, texture_id = context3d->createTexture());
    GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, texture_id));
    GLC(context3d, context3d->consumeTextureCHROMIUM(GL_TEXTURE_2D,
                                                     it->mailbox.name));
    ResourceId id = next_id_++;
    Resource resource(texture_id, it->size, it->format, it->filter);
    resource.mailbox.SetName(it->mailbox);
    // Don't allocate a texture for a child.
    resource.allocated = true;
    resources_[id] = resource;
    child_info.parent_to_child_map[id] = it->id;
    child_info.child_to_parent_map[it->id] = id;
  }
}

void ResourceProvider::ReceiveFromParent(
    const TransferableResourceArray& resources) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  if (!context3d || !context3d->makeContextCurrent()) {
    // FIXME: Implement this path for software compositing.
    return;
  }
  for (TransferableResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    ResourceMap::iterator map_iterator = resources_.find(it->id);
    DCHECK(map_iterator != resources_.end());
    Resource* resource = &map_iterator->second;
    DCHECK(resource->exported);
    resource->exported = false;
    resource->filter = it->filter;
    DCHECK(resource->mailbox.Equals(it->mailbox));
    if (resource->gl_id) {
      if (it->sync_point)
        GLC(context3d, context3d->waitSyncPoint(it->sync_point));
      GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, resource->gl_id));
      GLC(context3d, context3d->consumeTextureCHROMIUM(GL_TEXTURE_2D,
                                                       it->mailbox.name));
    } else {
      resource->mailbox = TextureMailbox(resource->mailbox.name(),
                                         resource->mailbox.callback(),
                                         it->sync_point);
    }
    if (resource->marked_for_deletion)
      DeleteResourceInternal(map_iterator);
  }
}

bool ResourceProvider::TransferResource(WebGraphicsContext3D* context,
                                        ResourceId id,
                                        TransferableResource* resource) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* source = &it->second;
  DCHECK(!source->locked_for_write);
  DCHECK(!source->lock_for_read_count);
  DCHECK(!source->external || (source->external && !source->mailbox.IsEmpty()));
  DCHECK(source->allocated);
  if (source->exported)
    return false;
  resource->id = id;
  resource->format = source->format;
  resource->filter = source->filter;
  resource->size = source->size;

  if (source->mailbox.IsEmpty()) {
    GLC(context3d, context3d->genMailboxCHROMIUM(resource->mailbox.name));
    source->mailbox.SetName(resource->mailbox);
  } else {
    resource->mailbox = source->mailbox.name();
  }

  if (source->gl_id) {
    GLC(context, context->bindTexture(GL_TEXTURE_2D, source->gl_id));
    GLC(context, context->produceTextureCHROMIUM(GL_TEXTURE_2D,
                                                 resource->mailbox.name));
  } else {
    resource->sync_point = source->mailbox.sync_point();
    source->mailbox.ResetSyncPoint();
  }
  return true;
}

void ResourceProvider::AcquirePixelBuffer(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->external);
  DCHECK(!resource->exported);

  if (resource->gl_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    if (!resource->gl_pixel_buffer_id)
      resource->gl_pixel_buffer_id = context3d->createBuffer();
    context3d->bindBuffer(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
        resource->gl_pixel_buffer_id);
    context3d->bufferData(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
        resource->size.width() * resource->size.height() * 4,
        NULL,
        GL_DYNAMIC_DRAW);
    context3d->bindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  if (resource->pixels) {
    if (resource->pixel_buffer)
      return;

    resource->pixel_buffer = new uint8_t[
        resource->size.width() * resource->size.height() * 4];
  }
}

void ResourceProvider::ReleasePixelBuffer(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->external);
  DCHECK(!resource->exported);

  if (resource->gl_id) {
    DCHECK(resource->gl_pixel_buffer_id);
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    context3d->bindBuffer(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
        resource->gl_pixel_buffer_id);
    context3d->bufferData(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
        0,
        NULL,
        GL_DYNAMIC_DRAW);
    context3d->bindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  if (resource->pixels) {
    if (!resource->pixel_buffer)
      return;
    delete[] resource->pixel_buffer;
    resource->pixel_buffer = NULL;
  }
}

uint8_t* ResourceProvider::MapPixelBuffer(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->external);
  DCHECK(!resource->exported);

  if (resource->gl_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    DCHECK(resource->gl_pixel_buffer_id);
    context3d->bindBuffer(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
        resource->gl_pixel_buffer_id);
    uint8_t* image = static_cast<uint8_t*>(
        context3d->mapBufferCHROMIUM(
            GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, GL_WRITE_ONLY));
    context3d->bindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
    DCHECK(image);
    return image;
  }

  if (resource->pixels)
    return resource->pixel_buffer;

  return NULL;
}

void ResourceProvider::UnmapPixelBuffer(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->external);
  DCHECK(!resource->exported);

  if (resource->gl_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    DCHECK(resource->gl_pixel_buffer_id);
    context3d->bindBuffer(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
        resource->gl_pixel_buffer_id);
    context3d->unmapBufferCHROMIUM(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM);
    context3d->bindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }
}

void ResourceProvider::SetPixelsFromBuffer(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->locked_for_write);
  DCHECK(!resource->lock_for_read_count);
  DCHECK(!resource->external);
  DCHECK(!resource->exported);
  DCHECK(ReadLockFenceHasPassed(resource));
  LazyAllocate(resource);

  if (resource->gl_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    DCHECK(resource->gl_pixel_buffer_id);
    context3d->bindTexture(GL_TEXTURE_2D, resource->gl_id);
    context3d->bindBuffer(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
        resource->gl_pixel_buffer_id);
    context3d->texSubImage2D(GL_TEXTURE_2D,
                             0, /* level */
                             0, /* x */
                             0, /* y */
                             resource->size.width(),
                             resource->size.height(),
                             resource->format,
                             GL_UNSIGNED_BYTE,
                             NULL);
    context3d->bindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  if (resource->pixels) {
    DCHECK(resource->pixel_buffer);
    DCHECK(resource->format == GL_RGBA);
    SkBitmap src;
    src.setConfig(SkBitmap::kARGB_8888_Config,
                  resource->size.width(),
                  resource->size.height());
    src.setPixels(resource->pixel_buffer);

    ScopedWriteLockSoftware lock(this, id);
    SkCanvas* dest = lock.sk_canvas();
    dest->writePixels(src, 0, 0);
  }
}

void ResourceProvider::BindForSampling(ResourceProvider::ResourceId resource_id,
                                       GLenum target, GLenum filter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  ResourceMap::iterator it = resources_.find(resource_id);
  DCHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(resource->lock_for_read_count);
  DCHECK(!resource->locked_for_write || resource->set_pixels_completion_forced);

  GLC(context3d, context3d->bindTexture(target, resource->gl_id));
  if (filter != resource->filter) {
    GLC(context3d, context3d->texParameteri(target,
                                            GL_TEXTURE_MIN_FILTER,
                                            filter));
    GLC(context3d, context3d->texParameteri(target,
                                            GL_TEXTURE_MAG_FILTER,
                                            filter));
    resource->filter = filter;
  }
}

void ResourceProvider::BeginSetPixels(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->pending_set_pixels);
  DCHECK(resource->gl_id || resource->allocated);
  DCHECK(ReadLockFenceHasPassed(resource));

  bool allocate = !resource->allocated;
  resource->allocated = true;
  LockForWrite(id);

  if (resource->gl_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    DCHECK(resource->gl_pixel_buffer_id);
    context3d->bindTexture(GL_TEXTURE_2D, resource->gl_id);
    context3d->bindBuffer(
        GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
        resource->gl_pixel_buffer_id);
    if (!resource->gl_upload_query_id)
      resource->gl_upload_query_id = context3d->createQueryEXT();
    context3d->beginQueryEXT(
        GL_ASYNC_PIXEL_TRANSFERS_COMPLETED_CHROMIUM,
        resource->gl_upload_query_id);
    if (allocate) {
      context3d->asyncTexImage2DCHROMIUM(GL_TEXTURE_2D,
                                         0, /* level */
                                         resource->format,
                                         resource->size.width(),
                                         resource->size.height(),
                                         0, /* border */
                                         resource->format,
                                         GL_UNSIGNED_BYTE,
                                         NULL);
    } else {
      context3d->asyncTexSubImage2DCHROMIUM(GL_TEXTURE_2D,
                                            0, /* level */
                                            0, /* x */
                                            0, /* y */
                                            resource->size.width(),
                                            resource->size.height(),
                                            resource->format,
                                            GL_UNSIGNED_BYTE,
                                            NULL);
    }
    context3d->endQueryEXT(GL_ASYNC_PIXEL_TRANSFERS_COMPLETED_CHROMIUM);
    context3d->bindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  if (resource->pixels)
    SetPixelsFromBuffer(id);

  resource->pending_set_pixels = true;
  resource->set_pixels_completion_forced = false;
}

void ResourceProvider::ForceSetPixelsToComplete(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(resource->locked_for_write);
  DCHECK(resource->pending_set_pixels);
  DCHECK(!resource->set_pixels_completion_forced);

  if (resource->gl_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, resource->gl_id));
    GLC(context3d, context3d->waitAsyncTexImage2DCHROMIUM(GL_TEXTURE_2D));
    GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, 0));
  }

  resource->set_pixels_completion_forced = true;
}

bool ResourceProvider::DidSetPixelsComplete(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(resource->locked_for_write);
  DCHECK(resource->pending_set_pixels);

  if (resource->gl_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    DCHECK(resource->gl_upload_query_id);
    unsigned complete = 1;
    context3d->getQueryObjectuivEXT(
        resource->gl_upload_query_id,
        GL_QUERY_RESULT_AVAILABLE_EXT,
        &complete);
    if (!complete)
      return false;
  }

  resource->pending_set_pixels = false;
  UnlockForWrite(id);

  return true;
}

void ResourceProvider::AbortSetPixels(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(resource->locked_for_write);
  DCHECK(resource->pending_set_pixels);

  if (resource->gl_id) {
    WebGraphicsContext3D* context3d = output_surface_->context3d();
    DCHECK(context3d);
    DCHECK(resource->gl_upload_query_id);
    // CHROMIUM_async_pixel_transfers currently doesn't have a way to
    // abort an upload. The best we can do is delete the query and
    // the texture.
    context3d->deleteQueryEXT(resource->gl_upload_query_id);
    resource->gl_upload_query_id = 0;
    context3d->deleteTexture(resource->gl_id);
    resource->gl_id = CreateTextureId(context3d);
    resource->allocated = false;
  }

  resource->pending_set_pixels = false;
  UnlockForWrite(id);
}

void ResourceProvider::AllocateForTesting(ResourceId id) {
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  LazyAllocate(resource);
}

void ResourceProvider::LazyAllocate(Resource* resource) {
  DCHECK(resource);
  DCHECK(resource->gl_id || resource->allocated);

  if (resource->allocated || !resource->gl_id)
    return;
  resource->allocated = true;
  WebGraphicsContext3D* context3d = output_surface_->context3d();
  gfx::Size& size = resource->size;
  GLenum format = resource->format;
  GLC(context3d, context3d->bindTexture(GL_TEXTURE_2D, resource->gl_id));
  if (use_texture_storage_ext_ && IsTextureFormatSupportedForStorage(format)) {
    GLenum storage_format = TextureToStorageFormat(format);
    GLC(context3d, context3d->texStorage2DEXT(GL_TEXTURE_2D,
                                              1,
                                              storage_format,
                                              size.width(),
                                              size.height()));
  } else {
    GLC(context3d, context3d->texImage2D(GL_TEXTURE_2D,
                                         0,
                                         format,
                                         size.width(),
                                         size.height(),
                                         0,
                                         format,
                                         GL_UNSIGNED_BYTE,
                                         NULL));
  }
}

void ResourceProvider::EnableReadLockFences(ResourceProvider::ResourceId id,
                                            bool enable) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  resource->enable_read_lock_fences = enable;
}

}  // namespace cc

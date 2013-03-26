// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_RESOURCE_PROVIDER_H_

#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/output/texture_copier.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/resources/transferable_resource.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/size.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace gfx {
class Rect;
class Vector2d;
}

namespace cc {
class TextureUploader;

// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT ResourceProvider {
 public:
  typedef unsigned ResourceId;
  typedef std::vector<ResourceId> ResourceIdArray;
  typedef std::set<ResourceId> ResourceIdSet;
  typedef base::hash_map<ResourceId, ResourceId> ResourceIdMap;
  enum TextureUsageHint {
    TextureUsageAny,
    TextureUsageFramebuffer,
  };
  enum ResourceType {
    GLTexture = 1,
    Bitmap,
  };

  static scoped_ptr<ResourceProvider> Create(OutputSurface* output_surface);

  virtual ~ResourceProvider();

  WebKit::WebGraphicsContext3D* GraphicsContext3D();
  TextureCopier* texture_copier() const { return texture_copier_.get(); }
  int max_texture_size() const { return max_texture_size_; }
  GLenum best_texture_format() const { return best_texture_format_; }
  unsigned num_resources() const { return resources_.size(); }

  // Checks whether a resource is in use by a consumer.
  bool InUseByConsumer(ResourceId id);


  // Producer interface.

  void set_default_resource_type(ResourceType type) {
    default_resource_type_ = type;
  }
  ResourceType default_resource_type() const { return default_resource_type_; }
  ResourceType GetResourceType(ResourceId id);

  // Creates a resource of the default resource type.
  ResourceId CreateResource(gfx::Size size,
                            GLenum format,
                            TextureUsageHint hint);

  // Creates a resource which is tagged as being managed for GPU memory
  // accounting purposes.
  ResourceId CreateManagedResource(gfx::Size size,
                                   GLenum format,
                                   TextureUsageHint hint);

  // You can also explicitly create a specific resource type.
  ResourceId CreateGLTexture(gfx::Size size,
                             GLenum format,
                             GLenum texture_pool,
                             TextureUsageHint hint);

  ResourceId CreateBitmap(gfx::Size size);
  // Wraps an external texture into a GL resource.
  ResourceId CreateResourceFromExternalTexture(unsigned texture_id);

  // Wraps an external texture mailbox into a GL resource.
  ResourceId CreateResourceFromTextureMailbox(const TextureMailbox& mailbox);

  void DeleteResource(ResourceId id);

  // Update pixels from image, copying source_rect (in image) to dest_offset (in
  // the resource).
  void SetPixels(ResourceId id,
                 const uint8_t* image,
                 gfx::Rect image_rect,
                 gfx::Rect source_rect,
                 gfx::Vector2d dest_offset);

  // Check upload status.
  size_t NumBlockingUploads();
  void MarkPendingUploadsAsNonBlocking();
  double EstimatedUploadsPerSecond();
  void FlushUploads();
  void ReleaseCachedData();

  // Flush all context operations, kicking uploads and ensuring ordering with
  // respect to other contexts.
  void Flush();

  // Only flush the command buffer if supported.
  // Returns true if the shallow flush occurred, false otherwise.
  bool ShallowFlushIfSupported();

  // Creates accounting for a child. Returns a child ID.
  int CreateChild();

  // Destroys accounting for the child, deleting all accounted resources.
  void DestroyChild(int child);

  // Gets the child->parent resource ID map.
  const ResourceIdMap& GetChildToParentMap(int child) const;

  // Prepares resources to be transfered to the parent, moving them to
  // mailboxes and serializing meta-data into TransferableResources.
  // Resources are not removed from the ResourceProvider, but are marked as
  // "in use".
  void PrepareSendToParent(const ResourceIdArray& resources,
                           TransferableResourceArray* transferable_resources);

  // Prepares resources to be transfered back to the child, moving them to
  // mailboxes and serializing meta-data into TransferableResources.
  // Resources are removed from the ResourceProvider. Note: the resource IDs
  // passed are in the parent namespace and will be translated to the child
  // namespace when returned.
  void PrepareSendToChild(int child,
                          const ResourceIdArray& resources,
                          TransferableResourceArray* transferable_resources);

  // Receives resources from a child, moving them from mailboxes. Resource IDs
  // passed are in the child namespace, and will be translated to the parent
  // namespace, added to the child->parent map.
  // NOTE: if the sync_point is set on any TransferableResource, this will
  // wait on it.
  void ReceiveFromChild(
      int child, const TransferableResourceArray& transferable_resources);

  // Receives resources from the parent, moving them from mailboxes. Resource
  // IDs passed are in the child namespace.
  // NOTE: if the sync_point is set on any TransferableResource, this will
  // wait on it.
  void ReceiveFromParent(
      const TransferableResourceArray& transferable_resources);

  // Bind the given GL resource to a texture target for sampling using the
  // specified filter for both minification and magnification. The resource
  // must be locked for reading.
  void BindForSampling(ResourceProvider::ResourceId resource_id,
                       GLenum target,
                       GLenum filter);

  // The following lock classes are part of the ResourceProvider API and are
  // needed to read and write the resource contents. The user must ensure
  // that they only use GL locks on GL resources, etc, and this is enforced
  // by assertions.
  class CC_EXPORT ScopedReadLockGL {
   public:
    ScopedReadLockGL(ResourceProvider* resource_provider,
                     ResourceProvider::ResourceId resource_id);
    ~ScopedReadLockGL();

    unsigned texture_id() const { return texture_id_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    unsigned texture_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockGL);
  };

  class CC_EXPORT ScopedSamplerGL : public ScopedReadLockGL {
   public:
    ScopedSamplerGL(ResourceProvider* resource_provider,
                    ResourceProvider::ResourceId resource_id,
                    GLenum target,
                    GLenum filter);

   private:
    DISALLOW_COPY_AND_ASSIGN(ScopedSamplerGL);
  };

  class CC_EXPORT ScopedWriteLockGL {
   public:
    ScopedWriteLockGL(ResourceProvider* resource_provider,
                      ResourceProvider::ResourceId resource_id);
    ~ScopedWriteLockGL();

    unsigned texture_id() const { return texture_id_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    unsigned texture_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGL);
  };

  class CC_EXPORT ScopedReadLockSoftware {
   public:
    ScopedReadLockSoftware(ResourceProvider* resource_provider,
                           ResourceProvider::ResourceId resource_id);
    ~ScopedReadLockSoftware();

    const SkBitmap* sk_bitmap() const { return &sk_bitmap_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    SkBitmap sk_bitmap_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSoftware);
  };

  class CC_EXPORT ScopedWriteLockSoftware {
   public:
    ScopedWriteLockSoftware(ResourceProvider* resource_provider,
                            ResourceProvider::ResourceId resource_id);
    ~ScopedWriteLockSoftware();

    SkCanvas* sk_canvas() { return sk_canvas_.get(); }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    SkBitmap sk_bitmap_;
    scoped_ptr<SkCanvas> sk_canvas_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockSoftware);
  };

  class Fence : public base::RefCounted<Fence> {
   public:
    Fence() {}
    virtual bool HasPassed() = 0;

   protected:
    friend class base::RefCounted<Fence>;
    virtual ~Fence() {}

    DISALLOW_COPY_AND_ASSIGN(Fence);
  };

  // Acquire pixel buffer for resource. The pixel buffer can be used to
  // set resource pixels without performing unnecessary copying.
  void AcquirePixelBuffer(ResourceId id);
  void ReleasePixelBuffer(ResourceId id);

  // Map/unmap the acquired pixel buffer.
  uint8_t* MapPixelBuffer(ResourceId id);
  void UnmapPixelBuffer(ResourceId id);

  // Update pixels from acquired pixel buffer.
  void SetPixelsFromBuffer(ResourceId id);

  // Asynchronously update pixels from acquired pixel buffer.
  void BeginSetPixels(ResourceId id);
  void ForceSetPixelsToComplete(ResourceId id);
  bool DidSetPixelsComplete(ResourceId id);
  void AbortSetPixels(ResourceId id);

  // For tests only! This prevents detecting uninitialized reads.
  // Use SetPixels or LockForWrite to allocate implicitly.
  void AllocateForTesting(ResourceId id);

  // Sets the current read fence. If a resource is locked for read
  // and has read fences enabled, the resource will not allow writes
  // until this fence has passed.
  void SetReadLockFence(scoped_refptr<Fence> fence) {
    current_read_lock_fence_ = fence;
  }
  Fence* GetReadLockFence() { return current_read_lock_fence_; }

  // Enable read lock fences for a specific resource.
  void EnableReadLockFences(ResourceProvider::ResourceId id, bool enable);

  // Indicates if we can currently lock this resource for write.
  bool CanLockForWrite(ResourceId id);

  cc::ContextProvider* offscreen_context_provider() {
    return offscreen_context_provider_.get();
  }
  void set_offscreen_context_provider(
      scoped_refptr<cc::ContextProvider> offscreen_context_provider) {
    offscreen_context_provider_ = offscreen_context_provider;
  }

 private:
  struct Resource {
    Resource();
    ~Resource();
    Resource(unsigned texture_id, gfx::Size size, GLenum format, GLenum filter);
    Resource(uint8_t* pixels, gfx::Size size, GLenum format, GLenum filter);

    unsigned gl_id;
    // Pixel buffer used for set pixels without unnecessary copying.
    unsigned gl_pixel_buffer_id;
    // Query used to determine when asynchronous set pixels complete.
    unsigned gl_upload_query_id;
    TextureMailbox mailbox;
    uint8_t* pixels;
    uint8_t* pixel_buffer;
    int lock_for_read_count;
    bool locked_for_write;
    bool external;
    bool exported;
    bool marked_for_deletion;
    bool pending_set_pixels;
    bool set_pixels_completion_forced;
    bool allocated;
    bool enable_read_lock_fences;
    scoped_refptr<Fence> read_lock_fence;
    gfx::Size size;
    GLenum format;
    // TODO(skyostil): Use a separate sampler object for filter state.
    GLenum filter;
    ResourceType type;
  };
  typedef base::hash_map<ResourceId, Resource> ResourceMap;
  struct Child {
    Child();
    ~Child();

    ResourceIdMap child_to_parent_map;
    ResourceIdMap parent_to_child_map;
  };
  typedef base::hash_map<int, Child> ChildMap;

  bool ReadLockFenceHasPassed(Resource* resource) {
    return !resource->read_lock_fence ||
        resource->read_lock_fence->HasPassed();
  }

  explicit ResourceProvider(OutputSurface* output_surface);
  bool Initialize();

  const Resource* LockForRead(ResourceId id);
  void UnlockForRead(ResourceId id);
  const Resource* LockForWrite(ResourceId id);
  void UnlockForWrite(ResourceId id);
  static void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                           const Resource* resource);

  bool TransferResource(WebKit::WebGraphicsContext3D* context,
                        ResourceId id,
                        TransferableResource* resource);
  void DeleteResourceInternal(ResourceMap::iterator it);
  void LazyAllocate(Resource* resource);

  OutputSurface* output_surface_;
  ResourceId next_id_;
  ResourceMap resources_;
  int next_child_;
  ChildMap children_;

  ResourceType default_resource_type_;
  bool use_texture_storage_ext_;
  bool use_texture_usage_hint_;
  bool use_shallow_flush_;
  scoped_ptr<TextureUploader> texture_uploader_;
  scoped_ptr<AcceleratedTextureCopier> texture_copier_;
  int max_texture_size_;
  GLenum best_texture_format_;

  scoped_refptr<cc::ContextProvider> offscreen_context_provider_;

  base::ThreadChecker thread_checker_;

  scoped_refptr<Fence> current_read_lock_fence_;

  DISALLOW_COPY_AND_ASSIGN(ResourceProvider);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_PROVIDER_H_

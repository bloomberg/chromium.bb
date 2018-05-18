// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_DISPLAY_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_DISPLAY_RESOURCE_PROVIDER_H_

#include <stddef.h>
#include <map>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "cc/cc_export.h"
#include "cc/output/overlay_candidate.h"
#include "components/viz/common/resources/resource.h"
#include "components/viz/common/resources/resource_fence.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/resource_metadata.h"
#include "components/viz/common/resources/return_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class ColorSpace;
class Size;
}  // namespace gfx

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {
class ContextProvider;
class SharedBitmapManager;
}  // namespace viz

namespace cc {

// This class provides abstractions for receiving and using resources from other
// modules/threads/processes. It abstracts away GL textures vs GpuMemoryBuffers
// vs software bitmaps behind a single ResourceId so that code in common can
// hold onto ResourceIds, as long as the code using them knows the correct type.
// It accepts as input TransferableResources which it holds internally, tracks
// state on, and exposes as a ResourceId.
//
// The resource's underlying type is accessed through locks that help to
// scope and safeguard correct usage with DCHECKs.
//
// This class is not thread-safe and can only be called from the thread it was
// created on.
class CC_EXPORT DisplayResourceProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  DisplayResourceProvider(viz::ContextProvider* compositor_context_provider,
                          viz::SharedBitmapManager* shared_bitmap_manager);
  ~DisplayResourceProvider() override;

  bool IsSoftware() const { return !compositor_context_provider_; }
  void DidLoseContextProvider() { lost_context_provider_ = true; }
  size_t num_resources() const { return resources_.size(); }

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

#if defined(OS_ANDROID)
  // Send an overlay promotion hint to all resources that requested it via
  // |wants_promotion_hints_set_|.  |promotable_hints| contains all the
  // resources that should be told that they're promotable.  Others will be told
  // that they're not promotable right now.
  void SendPromotionHints(
      const OverlayCandidateList::PromotionHintInfoMap& promotion_hints);

  // Indicates if this resource is backed by an Android SurfaceTexture, and thus
  // can't really be promoted to an overlay.
  bool IsBackedBySurfaceTexture(viz::ResourceId id);

  // Indicates if this resource wants to receive promotion hints.
  bool WantsPromotionHintForTesting(viz::ResourceId id);

  // Return the number of resources that request promotion hints.
  size_t CountPromotionHintRequestsForTesting();
#endif

  viz::ResourceType GetResourceType(viz::ResourceId id);
  GLenum GetResourceTextureTarget(viz::ResourceId id);
  // Return the format of the underlying buffer that can be used for scanout.
  gfx::BufferFormat GetBufferFormat(viz::ResourceId id);
  // Indicates if this resource may be used for a hardware overlay plane.
  bool IsOverlayCandidate(viz::ResourceId id);

  void WaitSyncToken(viz::ResourceId id);

  // Checks whether a resource is in use.
  bool InUse(viz::ResourceId id);

  // The following lock classes are part of the DisplayResourceProvider API and
  // are needed to read the resource contents. The user must ensure that they
  // only use GL locks on GL resources, etc, and this is enforced by assertions.
  class CC_EXPORT ScopedReadLockGL {
   public:
    ScopedReadLockGL(DisplayResourceProvider* resource_provider,
                     viz::ResourceId resource_id);
    ~ScopedReadLockGL();

    GLuint texture_id() const { return texture_id_; }
    GLenum target() const { return target_; }
    const gfx::Size& size() const { return size_; }
    const gfx::ColorSpace& color_space() const { return color_space_; }

   private:
    DisplayResourceProvider* const resource_provider_;
    const viz::ResourceId resource_id_;

    GLuint texture_id_ = 0;
    GLenum target_ = GL_TEXTURE_2D;
    gfx::Size size_;
    gfx::ColorSpace color_space_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockGL);
  };

  class CC_EXPORT ScopedSamplerGL {
   public:
    ScopedSamplerGL(DisplayResourceProvider* resource_provider,
                    viz::ResourceId resource_id,
                    GLenum filter);
    ScopedSamplerGL(DisplayResourceProvider* resource_provider,
                    viz::ResourceId resource_id,
                    GLenum unit,
                    GLenum filter);
    ~ScopedSamplerGL();

    GLuint texture_id() const { return resource_lock_.texture_id(); }
    GLenum target() const { return target_; }
    const gfx::ColorSpace& color_space() const {
      return resource_lock_.color_space();
    }

   private:
    const ScopedReadLockGL resource_lock_;
    const GLenum unit_;
    const GLenum target_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSamplerGL);
  };

  class CC_EXPORT ScopedReadLockSkImage {
   public:
    ScopedReadLockSkImage(DisplayResourceProvider* resource_provider,
                          viz::ResourceId resource_id);
    ~ScopedReadLockSkImage();

    const SkImage* sk_image() const { return sk_image_.get(); }

    bool valid() const { return !!sk_image_; }

   private:
    DisplayResourceProvider* const resource_provider_;
    const viz::ResourceId resource_id_;
    sk_sp<SkImage> sk_image_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSkImage);
  };

  class CC_EXPORT ScopedReadLockSoftware {
   public:
    ScopedReadLockSoftware(DisplayResourceProvider* resource_provider,
                           viz::ResourceId resource_id);
    ~ScopedReadLockSoftware();

    const SkBitmap* sk_bitmap() const {
      DCHECK(valid());
      return &sk_bitmap_;
    }

    bool valid() const { return !!sk_bitmap_.getPixels(); }

   private:
    DisplayResourceProvider* const resource_provider_;
    const viz::ResourceId resource_id_;
    SkBitmap sk_bitmap_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSoftware);
  };

  // Maintains set of lock for external use.
  class CC_EXPORT LockSetForExternalUse {
   public:
    explicit LockSetForExternalUse(DisplayResourceProvider* resource_provider);
    ~LockSetForExternalUse();

    // Lock a resource for external use.
    viz::ResourceMetadata LockResource(viz::ResourceId resource_id);

    // Unlock all locked resources with a |sync_token|.
    // See UnlockForExternalUse for the detail. All resources must be unlocked
    // before destroying this class.
    void UnlockResources(const gpu::SyncToken& sync_token);

   private:
    DisplayResourceProvider* const resource_provider_;
    std::vector<viz::ResourceId> resources_;

    DISALLOW_COPY_AND_ASSIGN(LockSetForExternalUse);
  };

  // All resources that are returned to children while an instance of this
  // class exists will be stored and returned when the instance is destroyed.
  class CC_EXPORT ScopedBatchReturnResources {
   public:
    explicit ScopedBatchReturnResources(
        DisplayResourceProvider* resource_provider);
    ~ScopedBatchReturnResources();

   private:
    DisplayResourceProvider* const resource_provider_;

    DISALLOW_COPY_AND_ASSIGN(ScopedBatchReturnResources);
  };

  class CC_EXPORT SynchronousFence : public viz::ResourceFence {
   public:
    explicit SynchronousFence(gpu::gles2::GLES2Interface* gl);

    // viz::ResourceFence implementation.
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

  // Sets the current read fence. If a resource is locked for read
  // and has read fences enabled, the resource will not allow writes
  // until this fence has passed.
  void SetReadLockFence(viz::ResourceFence* fence) {
    current_read_lock_fence_ = fence;
  }

  // Creates accounting for a child. Returns a child ID.
  int CreateChild(const viz::ReturnCallback& return_callback);

  // Destroys accounting for the child, deleting all accounted resources.
  void DestroyChild(int child);

  // Sets whether resources need sync points set on them when returned to this
  // child. Defaults to true.
  void SetChildNeedsSyncTokens(int child, bool needs_sync_tokens);

  // Gets the child->parent resource ID map.
  const std::unordered_map<viz::ResourceId, viz::ResourceId>&
  GetChildToParentMap(int child) const;

  // Receives resources from a child, moving them from mailboxes. ResourceIds
  // passed are in the child namespace, and will be translated to the parent
  // namespace, added to the child->parent map.
  // This adds the resources to the working set in the ResourceProvider without
  // declaring which resources are in use. Use DeclareUsedResourcesFromChild
  // after calling this method to do that. All calls to ReceiveFromChild should
  // be followed by a DeclareUsedResourcesFromChild.
  // NOTE: if the sync_token is set on any viz::TransferableResource, this will
  // wait on it.
  void ReceiveFromChild(
      int child,
      const std::vector<viz::TransferableResource>& transferable_resources);

  // Once a set of resources have been received, they may or may not be used.
  // This declares what set of resources are currently in use from the child,
  // releasing any other resources back to the child.
  void DeclareUsedResourcesFromChild(
      int child,
      const viz::ResourceIdSet& resources_from_child);

 private:
  struct Child {
    Child();
    Child(const Child& other);
    ~Child();

    std::unordered_map<viz::ResourceId, viz::ResourceId> child_to_parent_map;
    viz::ReturnCallback return_callback;
    bool marked_for_deletion = false;
    bool needs_sync_tokens = true;
  };

  enum DeleteStyle {
    NORMAL,
    FOR_SHUTDOWN,
  };

  using ChildMap = std::unordered_map<int, Child>;
  using ResourceMap =
      std::unordered_map<viz::ResourceId, viz::internal::Resource>;

  viz::internal::Resource* InsertResource(viz::ResourceId id,
                                          viz::internal::Resource resource);
  viz::internal::Resource* GetResource(viz::ResourceId id);

  // TODO(ericrk): TryGetResource is part of a temporary workaround for cases
  // where resources which should be available are missing. This version may
  // return nullptr if a resource is not found. https://crbug.com/811858
  viz::internal::Resource* TryGetResource(viz::ResourceId id);

  void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                    const viz::internal::Resource* resource);

  void DeleteResourceInternal(ResourceMap::iterator it, DeleteStyle style);

  void WaitSyncTokenInternal(viz::internal::Resource* resource);

  // Returns null if we do not have a viz::ContextProvider.
  gpu::gles2::GLES2Interface* ContextGL() const;

  const viz::internal::Resource* LockForRead(viz::ResourceId id);
  void UnlockForRead(viz::ResourceId id);

  // Lock a resource for external use.
  viz::ResourceMetadata LockForExternalUse(viz::ResourceId id);

  // Unlock a resource which locked by LockForExternalUse.
  // The |sync_token| should be waited on before reusing the resouce's backing
  // to ensure that any external use of it is completed. This |sync_token|
  // should have been verified.
  void UnlockForExternalUse(viz::ResourceId id,
                            const gpu::SyncToken& sync_token);

  void TryReleaseResource(ResourceMap::iterator it);
  // Binds the given GL resource to a texture target for sampling using the
  // specified filter for both minification and magnification. Returns the
  // texture target used. The resource must be locked for reading.
  GLenum BindForSampling(viz::ResourceId resource_id,
                         GLenum unit,
                         GLenum filter);
  bool ReadLockFenceHasPassed(const viz::internal::Resource* resource);
#if defined(OS_ANDROID)
  void DeletePromotionHint(ResourceMap::iterator it, DeleteStyle style);
#endif

  void DeleteAndReturnUnusedResourcesToChild(
      ChildMap::iterator child_it,
      DeleteStyle style,
      const std::vector<viz::ResourceId>& unused);
  void DestroyChildInternal(ChildMap::iterator it, DeleteStyle style);

  void SetBatchReturnResources(bool aggregate);

  THREAD_CHECKER(thread_checker_);
  viz::ContextProvider* const compositor_context_provider_;
  viz::SharedBitmapManager* const shared_bitmap_manager_;

  ResourceMap resources_;
  ChildMap children_;
  base::flat_map<viz::ResourceId, sk_sp<SkImage>> resource_sk_image_;
  // Maps from a child id to the set of resources to be returned to it.
  base::small_map<std::map<int, std::vector<viz::ResourceId>>>
      batched_returning_resources_;
  scoped_refptr<viz::ResourceFence> current_read_lock_fence_;
  // Keep track of whether deleted resources should be batched up or returned
  // immediately.
  bool batch_return_resources_ = false;
  // Set to true when the ContextProvider becomes lost, to inform that resources
  // modified by this class are now in an indeterminate state.
  bool lost_context_provider_ = false;
  // The ResourceIds in DisplayResourceProvider start from 2 to avoid
  // conflicts with id from LayerTreeResourceProvider.
  viz::ResourceId next_id_ = 2;
  // Used as child id when creating a child.
  int next_child_ = 1;
  // A process-unique ID used for disambiguating memory dumps from different
  // resource providers.
  int tracing_id_;

#if defined(OS_ANDROID)
  // Set of ResourceIds that would like to be notified about promotion hints.
  viz::ResourceIdSet wants_promotion_hints_set_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DisplayResourceProvider);
};

}  // namespace cc

#endif  // CC_RESOURCES_DISPLAY_RESOURCE_PROVIDER_H_

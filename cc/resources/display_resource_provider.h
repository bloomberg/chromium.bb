// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_DISPLAY_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_DISPLAY_RESOURCE_PROVIDER_H_

#include "build/build_config.h"
#include "cc/resources/resource_provider.h"

namespace viz {
class SharedBitmapManager;
}  // namespace viz

namespace cc {
class BlockingTaskRunner;

// This class is not thread-safe and can only be called from the thread it was
// created on.
class CC_EXPORT DisplayResourceProvider : public ResourceProvider {
 public:
  DisplayResourceProvider(
      viz::ContextProvider* compositor_context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      BlockingTaskRunner* blocking_main_thread_task_runner,
      bool delegated_sync_points_required,
      bool enable_color_correct_rasterization,
      const viz::ResourceSettings& resource_settings);
  ~DisplayResourceProvider() override;

#if defined(OS_ANDROID)
  // Send an overlay promotion hint to all resources that requested it via
  // |wants_promotion_hints_set_|.  |promotable_hints| contains all the
  // resources that should be told that they're promotable.  Others will be told
  // that they're not promotable right now.
  void SendPromotionHints(
      const OverlayCandidateList::PromotionHintInfoMap& promotion_hints);
#endif

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

  // Creates accounting for a child. Returns a child ID.
  int CreateChild(const ReturnCallback& return_callback);

  // Destroys accounting for the child, deleting all accounted resources.
  void DestroyChild(int child);

  // Sets whether resources need sync points set on them when returned to this
  // child. Defaults to true.
  void SetChildNeedsSyncTokens(int child, bool needs_sync_tokens);

  // Gets the child->parent resource ID map.
  const ResourceIdMap& GetChildToParentMap(int child) const;

  // Receives resources from a child, moving them from mailboxes. Resource IDs
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
  friend class ScopedBatchReturnResources;

  void SetBatchReturnResources(bool aggregate);

  DISALLOW_COPY_AND_ASSIGN(DisplayResourceProvider);
};

}  // namespace cc

#endif  // CC_RESOURCES_DISPLAY_RESOURCE_PROVIDER_H_

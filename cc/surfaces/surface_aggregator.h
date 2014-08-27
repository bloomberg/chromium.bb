// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_AGGREGATOR_H_
#define CC_SURFACES_SURFACE_AGGREGATOR_H_

#include <set>

#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/transferable_resource.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

class CompositorFrame;
class DelegatedFrameData;
class ResourceProvider;
class Surface;
class SurfaceDrawQuad;
class SurfaceManager;

class CC_SURFACES_EXPORT SurfaceAggregator {
 public:
  typedef base::hash_map<SurfaceId, int> SurfaceIndexMap;

  SurfaceAggregator(SurfaceManager* manager, ResourceProvider* provider);
  ~SurfaceAggregator();

  scoped_ptr<CompositorFrame> Aggregate(SurfaceId surface_id);
  SurfaceIndexMap& previous_contained_surfaces() {
    return previous_contained_surfaces_;
  }

 private:
  RenderPassId RemapPassId(RenderPassId surface_local_pass_id,
                           SurfaceId surface_id);

  void HandleSurfaceQuad(const SurfaceDrawQuad* surface_quad,
                         RenderPass* dest_pass);
  void CopySharedQuadState(const SharedQuadState* source_sqs,
                           const gfx::Transform& content_to_target_transform,
                           RenderPass* dest_render_pass);
  void CopyQuadsToPass(const QuadList& source_quad_list,
                       const SharedQuadStateList& source_shared_quad_state_list,
                       const gfx::Transform& content_to_target_transform,
                       RenderPass* dest_pass,
                       SurfaceId surface_id);
  void CopyPasses(const RenderPassList& source_pass_list,
                  const Surface* surface);

  bool TakeResources(Surface* surface,
                     const DelegatedFrameData* frame_data,
                     RenderPassList* render_pass_list);
  int ChildIdForSurface(Surface* surface);
  gfx::Rect DamageRectForSurface(const Surface* surface,
                                 const RenderPass& source);

  SurfaceManager* manager_;
  ResourceProvider* provider_;

  class RenderPassIdAllocator;
  typedef base::ScopedPtrHashMap<SurfaceId, RenderPassIdAllocator>
      RenderPassIdAllocatorMap;
  RenderPassIdAllocatorMap render_pass_allocator_map_;

  typedef base::hash_map<SurfaceId, int> SurfaceToResourceChildIdMap;
  SurfaceToResourceChildIdMap surface_id_to_resource_child_id_;

  // The following state is only valid for the duration of one Aggregate call
  // and is only stored on the class to avoid having to pass through every
  // function call.

  // This is the set of surfaces referenced in the aggregation so far, used to
  // detect cycles.
  typedef std::set<SurfaceId> SurfaceSet;
  SurfaceSet referenced_surfaces_;

  // For each Surface used in the last aggregation, gives the frame_index at
  // that time.
  SurfaceIndexMap previous_contained_surfaces_;
  SurfaceIndexMap contained_surfaces_;

  // This is the pass list for the aggregated frame.
  RenderPassList* dest_pass_list_;

  // Resource list for the aggregated frame.
  TransferableResourceArray* dest_resource_list_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceAggregator);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_AGGREGATOR_H_

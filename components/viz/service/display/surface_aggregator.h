// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_

#include <memory>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/quads/render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/color_space.h"

namespace cc {
class BlockingTaskRunner;
class CompositorFrame;
class DisplayResourceProvider;
class SurfaceDrawQuad;
}  // namespace cc

namespace viz {

class Surface;
class SurfaceClient;
class SurfaceManager;

class VIZ_SERVICE_EXPORT SurfaceAggregator {
 public:
  using SurfaceIndexMap = base::flat_map<SurfaceId, uint64_t>;

  SurfaceAggregator(SurfaceManager* manager,
                    cc::DisplayResourceProvider* provider,
                    bool aggregate_only_damaged);
  ~SurfaceAggregator();

  cc::CompositorFrame Aggregate(const SurfaceId& surface_id);
  void ReleaseResources(const SurfaceId& surface_id);
  SurfaceIndexMap& previous_contained_surfaces() {
    return previous_contained_surfaces_;
  }
  void SetFullDamageForSurface(const SurfaceId& surface_id);
  void set_output_is_secure(bool secure) { output_is_secure_ = secure; }

  // Set the color spaces for the created cc::RenderPasses, which is propagated
  // to the output surface.
  void SetOutputColorSpace(const gfx::ColorSpace& blending_color_space,
                           const gfx::ColorSpace& output_color_space);

 private:
  struct ClipData {
    ClipData() : is_clipped(false) {}
    ClipData(bool is_clipped, const gfx::Rect& rect)
        : is_clipped(is_clipped), rect(rect) {}

    bool is_clipped;
    gfx::Rect rect;
  };

  struct PrewalkResult {
    PrewalkResult();
    ~PrewalkResult();
    // This is the set of Surfaces that were referenced by another Surface, but
    // not included in a SurfaceDrawQuad.
    base::flat_set<SurfaceId> undrawn_surfaces;
    bool may_contain_video = false;
  };

  struct RenderPassInfo {
    // This is the id the pass is mapped to.
    int id;
    // This is true if the pass was used in the last aggregated frame.
    bool in_use = true;
  };

  struct SurfaceDrawQuadUmaStats {
    void Reset() {
      valid_surface = 0;
      missing_surface = 0;
      no_active_frame = 0;
    }

    // The surface exists and has an active frame.
    int valid_surface;
    // The surface doesn't exist.
    int missing_surface;
    // The surface exists but doesn't have an active frame.
    int no_active_frame;
  };

  ClipData CalculateClipRect(const ClipData& surface_clip,
                             const ClipData& quad_clip,
                             const gfx::Transform& target_transform);

  cc::RenderPassId RemapPassId(cc::RenderPassId surface_local_pass_id,
                               const SurfaceId& surface_id);

  void HandleSurfaceQuad(const cc::SurfaceDrawQuad* surface_quad,
                         const gfx::Transform& target_transform,
                         const ClipData& clip_rect,
                         cc::RenderPass* dest_pass,
                         bool ignore_undamaged,
                         gfx::Rect* damage_rect_in_quad_space,
                         bool* damage_rect_in_quad_space_valid);

  SharedQuadState* CopySharedQuadState(const SharedQuadState* source_sqs,
                                       const gfx::Transform& target_transform,
                                       const ClipData& clip_rect,
                                       cc::RenderPass* dest_render_pass);
  void CopyQuadsToPass(
      const cc::QuadList& source_quad_list,
      const cc::SharedQuadStateList& source_shared_quad_state_list,
      const std::unordered_map<ResourceId, ResourceId>& resource_to_child_map,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      cc::RenderPass* dest_pass,
      const SurfaceId& surface_id);
  gfx::Rect PrewalkTree(const SurfaceId& surface_id,
                        bool in_moved_pixel_surface,
                        int parent_pass,
                        PrewalkResult* result);
  void CopyUndrawnSurfaces(PrewalkResult* prewalk);
  void CopyPasses(const cc::CompositorFrame& frame, Surface* surface);
  void AddColorConversionPass();

  // Remove Surfaces that were referenced before but aren't currently
  // referenced from the ResourceProvider.
  // Also notifies SurfaceAggregatorClient of newly added and removed
  // child surfaces.
  void ProcessAddedAndRemovedSurfaces();

  void PropagateCopyRequestPasses();

  int ChildIdForSurface(Surface* surface);
  gfx::Rect DamageRectForSurface(const Surface* surface,
                                 const cc::RenderPass& source,
                                 const gfx::Rect& full_rect) const;

  static void UnrefResources(base::WeakPtr<SurfaceClient> surface_client,
                             const std::vector<ReturnedResource>& resources,
                             cc::BlockingTaskRunner* main_thread_task_runner);

  SurfaceManager* manager_;
  cc::DisplayResourceProvider* provider_;

  // Every Surface has its own cc::RenderPass ID namespace. This structure maps
  // each source (SurfaceId, cc::RenderPass id) to a unified ID namespace that's
  // used in the aggregated frame. An entry is removed from the map if it's not
  // used for one output frame.
  base::flat_map<std::pair<SurfaceId, cc::RenderPassId>, RenderPassInfo>
      render_pass_allocator_map_;
  cc::RenderPassId next_render_pass_id_;
  const bool aggregate_only_damaged_;
  bool output_is_secure_;

  // The color space for the root render pass. If this is different from
  // |blending_color_space_|, then a final render pass to convert between
  // the two will be added. This space must always be valid.
  gfx::ColorSpace output_color_space_ = gfx::ColorSpace::CreateSRGB();
  // The color space in which blending is done, used for all non-root render
  // passes. This space must always be valid.
  gfx::ColorSpace blending_color_space_ = gfx::ColorSpace::CreateSRGB();
  // The id for the final color conversion render pass.
  cc::RenderPassId color_conversion_render_pass_id_ = 0;

  base::flat_map<SurfaceId, int> surface_id_to_resource_child_id_;

  // The following state is only valid for the duration of one Aggregate call
  // and is only stored on the class to avoid having to pass through every
  // function call.

  // This is the set of surfaces referenced in the aggregation so far, used to
  // detect cycles.
  base::flat_set<SurfaceId> referenced_surfaces_;

  // For each Surface used in the last aggregation, gives the frame_index at
  // that time.
  SurfaceIndexMap previous_contained_surfaces_;
  SurfaceIndexMap contained_surfaces_;

  // After surface validation, every Surface in this set is valid.
  base::flat_set<SurfaceId> valid_surfaces_;

  // This is the pass list for the aggregated frame.
  cc::RenderPassList* dest_pass_list_;

  // This is the set of aggregated pass ids that are affected by filters that
  // move pixels.
  base::flat_set<cc::RenderPassId> moved_pixel_passes_;

  // This is the set of aggregated pass ids that are drawn by copy requests, so
  // should not have their damage rects clipped to the root damage rect.
  base::flat_set<cc::RenderPassId> copy_request_passes_;

  // This is the set of aggregated pass ids that has damage from contributing
  // content.
  base::flat_set<cc::RenderPassId> contributing_content_damaged_passes_;

  // This maps each aggregated pass id to the set of (aggregated) pass ids
  // that its cc::RenderPassDrawQuads depend on
  base::flat_map<cc::RenderPassId, base::flat_set<cc::RenderPassId>>
      render_pass_dependencies_;

  // The root damage rect of the currently-aggregating frame.
  gfx::Rect root_damage_rect_;

  // True if the frame that's currently being aggregated has copy requests.
  // This is valid during Aggregate after PrewalkTree is called.
  bool has_copy_requests_;

  // True if the frame that's currently being aggregated has cached render
  // passes. This is valid during Aggregate after PrewalkTree is called.
  bool has_cached_render_passes_;

  // Tracks UMA stats for SurfaceDrawQuads during a call to Aggregate().
  SurfaceDrawQuadUmaStats uma_stats_;

  base::WeakPtrFactory<SurfaceAggregator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceAggregator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_

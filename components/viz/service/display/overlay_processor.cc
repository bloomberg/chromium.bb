// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace viz {

namespace {

#if defined(OS_ANDROID)
// Utility class to make sure that we notify resource that they're promotable
// before returning from ProcessForOverlays.
class SendPromotionHintsBeforeReturning {
 public:
  SendPromotionHintsBeforeReturning(DisplayResourceProvider* resource_provider,
                                    OverlayCandidateList* candidates)
      : resource_provider_(resource_provider), candidates_(candidates) {}
  ~SendPromotionHintsBeforeReturning() {
    resource_provider_->SendPromotionHints(
        candidates_->promotion_hint_info_map_,
        candidates_->promotion_hint_requestor_set_);
  }

 private:
  DisplayResourceProvider* resource_provider_;
  OverlayCandidateList* candidates_;

  DISALLOW_COPY_AND_ASSIGN(SendPromotionHintsBeforeReturning);
};
#endif

}  // namespace

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnderlayDamage {
  kZeroDamageRect,
  kNonOccludingDamageOnly,
  kOccludingDamageOnly,
  kOccludingAndNonOccludingDamages,
  kMaxValue = kOccludingAndNonOccludingDamages,
};

// Record UMA histograms for overlays
// 1. Underlay vs. Overlay
// 2. Full screen mode vs. Non Full screen (Windows) mode
// 3. Overlay zero damage rect vs. non zero damage rect
// 4. Underlay zero damage rect, non-zero damage rect with non-occluding damage
//   only, non-zero damage rect with occluding damage, and non-zero damage rect
//   with both damages

// static
void OverlayProcessor::RecordOverlayDamageRectHistograms(
    bool is_overlay,
    bool has_occluding_surface_damage,
    bool zero_damage_rect,
    bool occluding_damage_equal_to_damage_rect) {
  if (is_overlay) {
    UMA_HISTOGRAM_BOOLEAN("Viz.DisplayCompositor.RootDamageRect.Overlay",
                          !zero_damage_rect);
  } else {  // underlay
    UnderlayDamage underlay_damage = UnderlayDamage::kZeroDamageRect;
    if (zero_damage_rect) {
      underlay_damage = UnderlayDamage::kZeroDamageRect;
    } else {
      if (has_occluding_surface_damage) {
        if (occluding_damage_equal_to_damage_rect) {
          underlay_damage = UnderlayDamage::kOccludingDamageOnly;
        } else {
          underlay_damage = UnderlayDamage::kOccludingAndNonOccludingDamages;
        }
      } else {
        underlay_damage = UnderlayDamage::kNonOccludingDamageOnly;
      }
    }
    UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.RootDamageRect.Underlay",
                              underlay_damage);
  }
}

OverlayStrategy OverlayProcessor::Strategy::GetUMAEnum() const {
  return OverlayStrategy::kUnknown;
}

OverlayProcessor::OverlayProcessor(OutputSurface* surface)
    : surface_(surface), dc_processor_(surface) {}

void OverlayProcessor::Initialize() {
  DCHECK(surface_);
  OverlayCandidateValidator* validator =
      surface_->GetOverlayCandidateValidator();
  if (validator)
    validator->GetStrategies(&strategies_);
}

OverlayProcessor::~OverlayProcessor() {}

gfx::Rect OverlayProcessor::GetAndResetOverlayDamage() {
  gfx::Rect result = overlay_damage_rect_;
  overlay_damage_rect_ = gfx::Rect();
  return result;
}

bool OverlayProcessor::ProcessForCALayers(
    DisplayResourceProvider* resource_provider,
    RenderPass* render_pass,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    OverlayCandidateList* overlay_candidates,
    CALayerOverlayList* ca_layer_overlays,
    gfx::Rect* damage_rect) {
  OverlayCandidateValidator* overlay_validator =
      surface_->GetOverlayCandidateValidator();
  if (!overlay_validator || !overlay_validator->AllowCALayerOverlays())
    return false;

  if (!ProcessForCALayerOverlays(
          resource_provider, gfx::RectF(render_pass->output_rect),
          render_pass->quad_list, render_pass_filters,
          render_pass_backdrop_filters, ca_layer_overlays))
    return false;

  // CALayer overlays are all-or-nothing. If all quads were replaced with
  // layers then clear the list and remove the backbuffer from the overcandidate
  // list.
  overlay_candidates->clear();
  overlay_damage_rect_ = render_pass->output_rect;
  *damage_rect = gfx::Rect();
  return true;
}

bool OverlayProcessor::ProcessForDCLayers(
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_passes,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    OverlayCandidateList* overlay_candidates,
    DCLayerOverlayList* dc_layer_overlays,
    gfx::Rect* damage_rect) {
  OverlayCandidateValidator* overlay_validator =
      surface_->GetOverlayCandidateValidator();
  if (!overlay_validator || !overlay_validator->AllowDCLayerOverlays())
    return false;

  dc_processor_.Process(resource_provider,
                        gfx::RectF(render_passes->back()->output_rect),
                        render_passes, damage_rect, dc_layer_overlays);

  DCHECK(overlay_candidates->empty());
  return true;
}

void OverlayProcessor::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_passes,
    const SkMatrix44& output_color_matrix,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    OverlayCandidateList* candidates,
    CALayerOverlayList* ca_layer_overlays,
    DCLayerOverlayList* dc_layer_overlays,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  TRACE_EVENT0("viz", "OverlayProcessor::ProcessForOverlays");
#if defined(OS_ANDROID)
  // Be sure to send out notifications, regardless of whether we get to
  // processing for overlays or not.  If we don't, then we should notify that
  // they are not promotable.
  SendPromotionHintsBeforeReturning notifier(resource_provider, candidates);
#endif

  // Reset |previous_frame_underlay_rect_| in case UpdateDamageRect() not being
  // invoked.  Also reset |previous_frame_underlay_was_unoccluded_|.
  const gfx::Rect previous_frame_underlay_rect = previous_frame_underlay_rect_;
  previous_frame_underlay_rect_ = gfx::Rect();
  bool previous_frame_underlay_was_unoccluded =
      previous_frame_underlay_was_unoccluded_;
  previous_frame_underlay_was_unoccluded_ = false;

  RenderPass* render_pass = render_passes->back().get();

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (!render_pass->copy_requests.empty()) {
    damage_rect->Union(
        dc_processor_.previous_frame_overlay_damage_contribution());
    // Update damage rect before calling ClearOverlayState, otherwise
    // previous_frame_overlay_rect_union will be empty.
    dc_processor_.ClearOverlayState();

    return;
  }

  // First attempt to process for CALayers.
  if (ProcessForCALayers(resource_provider, render_passes->back().get(),
                         render_pass_filters, render_pass_backdrop_filters,
                         candidates, ca_layer_overlays, damage_rect)) {
    return;
  }

  if (ProcessForDCLayers(resource_provider, render_passes, render_pass_filters,
                         render_pass_backdrop_filters, candidates,
                         dc_layer_overlays, damage_rect)) {
    return;
  }

  // Only if that fails, attempt hardware overlay strategies.
  Strategy* successful_strategy = nullptr;
  for (const auto& strategy : strategies_) {
    if (strategy->Attempt(output_color_matrix, render_pass_backdrop_filters,
                          resource_provider, render_passes, candidates,
                          content_bounds)) {
      successful_strategy = strategy.get();

      UpdateDamageRect(candidates, previous_frame_underlay_rect,
                       previous_frame_underlay_was_unoccluded,
                       &render_pass->quad_list, damage_rect);
      break;
    }
  }

  if (!successful_strategy && !previous_frame_underlay_rect.IsEmpty())
    damage_rect->Union(previous_frame_underlay_rect);

  UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy",
                            successful_strategy
                                ? successful_strategy->GetUMAEnum()
                                : OverlayStrategy::kNoStrategyUsed);

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());
}

// Subtract on-top opaque overlays from the damage rect, unless the overlays use
// the backbuffer as their content (in which case, add their combined rect
// back to the damage at the end).
// Also subtract unoccluded underlays from the damage rect if we know that the
// same underlay was scheduled on the previous frame. If the renderer decides
// not to swap the framebuffer there will still be a transparent hole in the
// previous frame.
void OverlayProcessor::UpdateDamageRect(
    OverlayCandidateList* candidates,
    const gfx::Rect& previous_frame_underlay_rect,
    bool previous_frame_underlay_was_unoccluded,
    const QuadList* quad_list,
    gfx::Rect* damage_rect) {
  gfx::Rect output_surface_overlay_damage_rect;
  gfx::Rect this_frame_underlay_rect;
  for (const OverlayCandidate& overlay : *candidates) {
    if (overlay.plane_z_order >= 0) {
      const gfx::Rect overlay_display_rect =
          ToEnclosedRect(overlay.display_rect);
      if (overlay.use_output_surface_for_resource) {
        if (overlay.plane_z_order > 0)
          output_surface_overlay_damage_rect.Union(overlay_display_rect);
      } else {
        overlay_damage_rect_.Union(overlay_display_rect);
        if (overlay.is_opaque)
          damage_rect->Subtract(overlay_display_rect);
      }
    } else {
      // Process underlay candidates:
      // Track the underlay_rect from frame to frame. If it is the same and
      // nothing is on top of it then that rect doesn't need to be damaged
      // because the drawing is occurring on a different plane. If it is
      // different then that indicates that a different underlay has been chosen
      // and the previous underlay rect should be damaged because it has changed
      // planes from the underlay plane to the main plane.
      // It then checks that this is not a transition from occluded to
      // unoccluded.
      //
      // We also insist that the underlay is unoccluded for at leat one frame,
      // else when content above the overlay transitions from not fully
      // transparent to fully transparent, we still need to erase it from the
      // framebuffer.  Otherwise, the last non-transparent frame will remain.
      // https://crbug.com/875879
      // However, if the underlay is unoccluded, we check if the damage is due
      // to a solid-opaque-transparent quad. If so, then we subtract this
      // damage.
      this_frame_underlay_rect = ToEnclosedRect(overlay.display_rect);

      bool same_underlay_rect =
          this_frame_underlay_rect == previous_frame_underlay_rect;
      bool transition_from_occluded_to_unoccluded =
          overlay.is_unoccluded && !previous_frame_underlay_was_unoccluded;
      bool always_unoccluded =
          overlay.is_unoccluded && previous_frame_underlay_was_unoccluded;
      bool has_damage = std::any_of(
          quad_list->begin(), quad_list->end(), [](const auto& quad) {
            bool solid_opaque_tranparent =
                !quad->ShouldDrawWithBlending() &&
                quad->material == DrawQuad::SOLID_COLOR &&
                SolidColorDrawQuad::MaterialCast(quad)->color ==
                    SK_ColorTRANSPARENT;

            return !solid_opaque_tranparent &&
                   quad->shared_quad_state->has_surface_damage;
          });

      if (same_underlay_rect && !transition_from_occluded_to_unoccluded &&
          (always_unoccluded || !has_damage)) {
        damage_rect->Subtract(this_frame_underlay_rect);
      }
      previous_frame_underlay_was_unoccluded_ = overlay.is_unoccluded;
    }
  }

  if (this_frame_underlay_rect != previous_frame_underlay_rect)
    damage_rect->Union(previous_frame_underlay_rect);

  previous_frame_underlay_rect_ = this_frame_underlay_rect;

  damage_rect->Union(output_surface_overlay_damage_rect);
}

}  // namespace viz

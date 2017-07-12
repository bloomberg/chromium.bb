// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_processor.h"

#include "cc/output/dc_layer_overlay.h"
#include "cc/output/output_surface.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "cc/quads/draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace {

#if defined(OS_ANDROID)
// Utility class to make sure that we notify resource that they're promotable
// before returning from ProcessForOverlays.
class SendPromotionHintsBeforeReturning {
 public:
  SendPromotionHintsBeforeReturning(cc::ResourceProvider* resource_provider,
                                    cc::OverlayCandidateList* candidates)
      : resource_provider_(resource_provider), candidates_(candidates) {}
  ~SendPromotionHintsBeforeReturning() {
    resource_provider_->SendPromotionHints(
        candidates_->promotion_hint_info_map_);
  }

 private:
  cc::ResourceProvider* resource_provider_;
  cc::OverlayCandidateList* candidates_;

  DISALLOW_COPY_AND_ASSIGN(SendPromotionHintsBeforeReturning);
};
#endif

}  // namespace

namespace cc {

OverlayProcessor::OverlayProcessor(OutputSurface* surface) : surface_(surface) {
}

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
    ResourceProvider* resource_provider,
    RenderPass* render_pass,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_background_filters,
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
          render_pass_background_filters, ca_layer_overlays))
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
    ResourceProvider* resource_provider,
    RenderPass* render_pass,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_background_filters,
    OverlayCandidateList* overlay_candidates,
    DCLayerOverlayList* dc_layer_overlays,
    gfx::Rect* damage_rect) {
  OverlayCandidateValidator* overlay_validator =
      surface_->GetOverlayCandidateValidator();
  if (!overlay_validator || !overlay_validator->AllowDCLayerOverlays())
    return false;

  dc_processor_.Process(resource_provider, gfx::RectF(render_pass->output_rect),
                        &render_pass->quad_list, &overlay_damage_rect_,
                        damage_rect, dc_layer_overlays);

  DCHECK(overlay_candidates->empty());
  return true;
}

void OverlayProcessor::ProcessForOverlays(
    ResourceProvider* resource_provider,
    RenderPass* render_pass,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_background_filters,
    OverlayCandidateList* candidates,
    CALayerOverlayList* ca_layer_overlays,
    DCLayerOverlayList* dc_layer_overlays,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
#if defined(OS_ANDROID)
  // Be sure to send out notifications, regardless of whether we get to
  // processing for overlays or not.  If we don't, then we should notify that
  // they are not promotable.
  SendPromotionHintsBeforeReturning notifier(resource_provider, candidates);
#endif

  // Reset |previous_frame_underlay_rect_| in case UpdateDamageRect() not being
  // invoked.
  const gfx::Rect previous_frame_underlay_rect = previous_frame_underlay_rect_;
  previous_frame_underlay_rect_ = gfx::Rect();

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (!render_pass->copy_requests.empty()) {
    dc_processor_.ClearOverlayState();
    return;
  }

  // First attempt to process for CALayers.
  if (ProcessForCALayers(resource_provider, render_pass, render_pass_filters,
                         render_pass_background_filters, candidates,
                         ca_layer_overlays, damage_rect)) {
    return;
  }

  if (ProcessForDCLayers(resource_provider, render_pass, render_pass_filters,
                         render_pass_background_filters, candidates,
                         dc_layer_overlays, damage_rect)) {
    return;
  }

  // Only if that fails, attempt hardware overlay strategies.
  for (const auto& strategy : strategies_) {
    if (!strategy->Attempt(resource_provider, render_pass, candidates,
                           content_bounds))
      continue;

    UpdateDamageRect(candidates, previous_frame_underlay_rect, damage_rect);
    return;
  }
}

// Subtract on-top opaque overlays from the damage rect, unless the overlays use
// the backbuffer as their content (in which case, add their combined rect
// back to the damage at the end).
// Also subtract unoccluded underlays from the damage rect if we know that the
// same underlay was scheduled on the previous frame. If the renderer decides
// not to swap the framebuffer there will still be a transparent hole in the
// previous frame. This only handles the common case of a single underlay quad
// for fullscreen video.
void OverlayProcessor::UpdateDamageRect(
    OverlayCandidateList* candidates,
    const gfx::Rect& previous_frame_underlay_rect,
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
    } else if (overlay.is_unoccluded && this_frame_underlay_rect.IsEmpty()) {
      this_frame_underlay_rect = ToEnclosedRect(overlay.display_rect);
    }
  }

  if (this_frame_underlay_rect == previous_frame_underlay_rect)
    damage_rect->Subtract(this_frame_underlay_rect);
  previous_frame_underlay_rect_ = this_frame_underlay_rect;

  damage_rect->Union(output_surface_overlay_damage_rect);
}

}  // namespace cc

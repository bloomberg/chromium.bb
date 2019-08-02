// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_H_

#include <vector>

#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class RendererSettings;
class ContextProvider;

// This class that can be used to answer questions about possible overlay
// configurations for a particular output device.
class VIZ_SERVICE_EXPORT OverlayCandidateValidator {
 public:
  static std::unique_ptr<OverlayCandidateValidator> Create(
      gpu::SurfaceHandle surface_handle,
      const ContextProvider* context_provider,
      const RendererSettings& renderer_settings);
  virtual ~OverlayCandidateValidator();

  // Populates a list of strategies that may work with this validator. Should be
  // called at most once.
  virtual void InitializeStrategies() {}

  // Returns true if draw quads can be represented as CALayers (Mac only).
  virtual bool AllowCALayerOverlays() const = 0;

  // Returns true if draw quads can be represented as Direct Composition
  // Visuals (Windows only).
  virtual bool AllowDCLayerOverlays() const = 0;

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  virtual bool NeedsSurfaceOccludingDamageRect() const = 0;

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited. Candidates with |overlay_handled| set to
  // true must also have their |display_rect| converted to integer
  // coordinates if necessary.
  virtual void CheckOverlaySupport(OverlayCandidateList* surfaces) = 0;

  // The OverlayCandidate for the OutputSurface. Allows the validator to update
  // any properties of the |surface| required by the platform.
  virtual void AdjustOutputSurfaceOverlay(OverlayCandidate* candidate) {}

  // Set the overlay display transform and viewport size. Value only used for
  // Android Surface Control.
  virtual void SetDisplayTransform(gfx::OverlayTransform transform) {}
  virtual void SetViewportSize(const gfx::Size& size) {}

  // Returns the overlay damage rect covering the main plane rendered by the
  // OutputSurface. This rect is in the same space where the OutputSurface
  // renders the content for the main plane, including the display transform if
  // needed.
  virtual gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& candidate) const;

  // Disables overlays when software mirroring display. This only needs to be
  // implemented for Chrome OS.
  virtual void SetSoftwareMirrorMode(bool enabled) {}

  // Iterate through a list of strategies and attempt to overlay with each.
  // Returns true if one of the attempts is successful. Has to be called after
  // InitializeStrategies().
  bool AttemptWithStrategies(
      const SkMatrix44& output_color_matrix,
      const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_pass_list,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds) const;

 protected:
  OverlayCandidateValidator();

  OverlayProcessor::StrategyList strategies_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_H_

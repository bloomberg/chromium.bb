// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_PROCESSOR_H_
#define CC_OUTPUT_OVERLAY_PROCESSOR_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "cc/cc_export.h"
#include "cc/output/ca_layer_overlay.h"
#include "cc/output/dc_layer_overlay.h"
#include "cc/output/overlay_candidate.h"
#include "cc/quads/render_pass.h"

namespace cc {
class DisplayResourceProvider;
class OutputSurface;

class CC_EXPORT OverlayProcessor {
 public:
  class CC_EXPORT Strategy {
   public:
    virtual ~Strategy() {}
    // Returns false if the strategy cannot be made to work with the
    // current set of render passes. Returns true if the strategy was successful
    // and adds any additional passes necessary to represent overlays to
    // |render_passes|.
    virtual bool Attempt(DisplayResourceProvider* resource_provider,
                         RenderPass* render_pass,
                         OverlayCandidateList* candidates,
                         std::vector<gfx::Rect>* content_bounds) = 0;
  };
  using StrategyList = std::vector<std::unique_ptr<Strategy>>;

  explicit OverlayProcessor(OutputSurface* surface);
  virtual ~OverlayProcessor();
  // Virtual to allow testing different strategies.
  virtual void Initialize();

  gfx::Rect GetAndResetOverlayDamage();

  using FilterOperationsMap = base::flat_map<RenderPassId, FilterOperations*>;

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_background_filters,
      OverlayCandidateList* overlay_candidates,
      CALayerOverlayList* ca_layer_overlays,
      DCLayerOverlayList* dc_layer_overlays,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds);

 protected:
  StrategyList strategies_;
  OutputSurface* surface_;
  gfx::Rect overlay_damage_rect_;
  gfx::Rect previous_frame_underlay_rect_;

 private:
  bool ProcessForCALayers(
      DisplayResourceProvider* resource_provider,
      RenderPass* render_pass,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_background_filters,
      OverlayCandidateList* overlay_candidates,
      CALayerOverlayList* ca_layer_overlays,
      gfx::Rect* damage_rect);
  bool ProcessForDCLayers(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_background_filters,
      OverlayCandidateList* overlay_candidates,
      DCLayerOverlayList* dc_layer_overlays,
      gfx::Rect* damage_rect);
  // Update |damage_rect| by removing damage casued by |candidates|.
  void UpdateDamageRect(OverlayCandidateList* candidates,
                        const gfx::Rect& previous_frame_underlay_rect,
                        gfx::Rect* damage_rect);

  DCLayerOverlayProcessor dc_processor_;

  DISALLOW_COPY_AND_ASSIGN(OverlayProcessor);
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_PROCESSOR_H_

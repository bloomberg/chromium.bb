// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_PROCESSOR_H_
#define CC_OUTPUT_OVERLAY_PROCESSOR_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/ca_layer_overlay.h"
#include "cc/output/overlay_candidate.h"
#include "cc/quads/render_pass.h"

namespace cc {
class OutputSurface;
class ResourceProvider;

class CC_EXPORT OverlayProcessor {
 public:
  class CC_EXPORT Strategy {
   public:
    virtual ~Strategy() {}
    // Returns false if the strategy cannot be made to work with the
    // current set of render passes. Returns true if the strategy was successful
    // and adds any additional passes necessary to represent overlays to
    // |render_passes|.
    virtual bool Attempt(ResourceProvider* resource_provider,
                         RenderPassList* render_passes,
                         OverlayCandidateList* candidates) = 0;
  };
  using StrategyList = std::vector<scoped_ptr<Strategy>>;

  explicit OverlayProcessor(OutputSurface* surface);
  virtual ~OverlayProcessor();
  // Virtual to allow testing different strategies.
  virtual void Initialize();

  gfx::Rect GetAndResetOverlayDamage();

  void ProcessForOverlays(ResourceProvider* resource_provider,
                          RenderPassList* render_passes,
                          OverlayCandidateList* overlay_candidates,
                          CALayerOverlayList* ca_layer_overlays,
                          gfx::Rect* damage_rect);

  // Notify the processor that ProcessForOverlays is being skipped this frame.
  void SkipProcessForOverlays();

 protected:
  StrategyList strategies_;
  OutputSurface* surface_;
  gfx::Rect overlay_damage_rect_;
  gfx::Rect previous_frame_underlay_rect_;

 private:
  bool ProcessForCALayers(ResourceProvider* resource_provider,
                          RenderPassList* render_passes,
                          OverlayCandidateList* overlay_candidates,
                          CALayerOverlayList* ca_layer_overlays,
                          gfx::Rect* damage_rect);
  // Update |damage_rect| by removing damage casued by |candidates|.
  void UpdateDamageRect(OverlayCandidateList* candidates,
                        gfx::Rect* damage_rect);

  DISALLOW_COPY_AND_ASSIGN(OverlayProcessor);
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_PROCESSOR_H_

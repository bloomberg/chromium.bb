// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_DAMAGE_TRACKER_H_
#define CC_TREES_DAMAGE_TRACKER_H_

#include <vector>
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_lists.h"
#include "ui/gfx/rect_f.h"

class SkImageFilter;

namespace gfx {
class Rect;
}

namespace WebKit {
class WebFilterOperations;
}

namespace cc {

class LayerImpl;
class RenderSurfaceImpl;

// Computes the region where pixels have actually changed on a
// RenderSurfaceImpl. This region is used to scissor what is actually drawn to
// the screen to save GPU computation and bandwidth.
class CC_EXPORT DamageTracker {
 public:
  static scoped_ptr<DamageTracker> Create();
  ~DamageTracker();

  void DidDrawDamagedArea() { current_damage_rect_ = gfx::RectF(); }
  void ForceFullDamageNextUpdate() { force_full_damage_next_update_ = true; }
  void UpdateDamageTrackingState(
      const LayerImplList& layer_list,
      int target_surface_layer_id,
      bool target_surface_property_changed_only_from_descendant,
      gfx::Rect target_surface_content_rect,
      LayerImpl* target_surface_mask_layer,
      const WebKit::WebFilterOperations& filters,
      SkImageFilter* filter);

  gfx::RectF current_damage_rect() { return current_damage_rect_; }

 private:
  DamageTracker();

  gfx::RectF TrackDamageFromActiveLayers(
      const LayerImplList& layer_list,
      int target_surface_layer_id);
  gfx::RectF TrackDamageFromSurfaceMask(LayerImpl* target_surface_mask_layer);
  gfx::RectF TrackDamageFromLeftoverRects();

  gfx::RectF RemoveRectFromCurrentFrame(int layer_id, bool* layer_is_new);
  void SaveRectForNextFrame(int layer_id, const gfx::RectF& target_space_rect);

  // These helper functions are used only in TrackDamageFromActiveLayers().
  void ExtendDamageForLayer(LayerImpl* layer, gfx::RectF* target_damage_rect);
  void ExtendDamageForRenderSurface(LayerImpl* layer,
                                    gfx::RectF* target_damage_rect);

  // To correctly track exposed regions, two hashtables of rects are maintained.
  // The "current" map is used to compute exposed regions of the current frame,
  // while the "next" map is used to collect layer rects that are used in the
  // next frame.
  typedef base::hash_map<int, gfx::RectF> RectMap;
  scoped_ptr<RectMap> current_rect_history_;
  scoped_ptr<RectMap> next_rect_history_;

  gfx::RectF current_damage_rect_;
  bool force_full_damage_next_update_;

  DISALLOW_COPY_AND_ASSIGN(DamageTracker);
};

}  // namespace cc

#endif  // CC_TREES_DAMAGE_TRACKER_H_

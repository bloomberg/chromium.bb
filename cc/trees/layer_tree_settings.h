// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_SETTINGS_H_
#define CC_TREES_LAYER_TREE_SETTINGS_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT LayerTreeSettings {
 public:
  LayerTreeSettings();
  ~LayerTreeSettings();

  bool accelerate_painting;
  bool compositor_frame_message;
  bool impl_side_painting;
  bool render_vsync_enabled;
  bool per_tile_painting_enabled;
  bool partial_swap_enabled;
  bool cache_render_pass_contents;
  bool right_aligned_scheduling_enabled;
  bool accelerated_animation_enabled;
  bool background_color_instead_of_checkerboard;
  bool show_overdraw_in_tracing;
  bool can_use_lcd_text;
  bool should_clear_root_render_pass;
  bool use_linear_fade_scrollbar_animator;
  bool solid_color_scrollbars;
  SkColor solid_color_scrollbar_color;
  int solid_color_scrollbar_thickness_dip;
  bool calculate_top_controls_position;
  bool use_cheapness_estimator;
  bool use_color_estimator;
  bool use_memory_management;
  bool prediction_benchmarking;
  float minimum_contents_scale;
  float low_res_contents_scale_factor;
  float top_controls_height;
  float top_controls_show_threshold;
  float top_controls_hide_threshold;
  double refresh_rate;
  size_t max_partial_texture_updates;
  size_t num_raster_threads;
  gfx::Size default_tile_size;
  gfx::Size max_untiled_layer_size;
  gfx::Size minimum_occlusion_tracking_size;
  bool use_pinch_zoom_scrollbars;

  LayerTreeDebugState initial_debug_state;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_SETTINGS_H_

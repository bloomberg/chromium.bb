// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_SETTINGS_H_
#define CC_TREES_LAYER_TREE_SETTINGS_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/output/renderer_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class CC_EXPORT LayerTreeSettings {
 public:
  LayerTreeSettings();
  ~LayerTreeSettings();

  RendererSettings renderer_settings;
  bool impl_side_painting;
  bool raster_enabled;
  bool throttle_frame_production;
  bool single_thread_proxy_scheduler;
  bool use_external_begin_frame_source;
  bool forward_begin_frames_to_children;
  bool main_frame_before_activation_enabled;
  bool using_synchronous_renderer_compositor;
  bool report_overscroll_only_for_scrollable_axes;
  bool per_tile_painting_enabled;
  bool accelerated_animation_enabled;
  bool can_use_lcd_text;
  bool use_distance_field_text;
  bool gpu_rasterization_enabled;
  bool gpu_rasterization_forced;
  int gpu_rasterization_msaa_sample_count;
  float gpu_rasterization_skewport_target_time_in_seconds;
  bool threaded_gpu_rasterization_enabled;
  bool create_low_res_tiling;

  enum ScrollbarAnimator {
    NO_ANIMATOR,
    LINEAR_FADE,
    THINNING,
  };
  ScrollbarAnimator scrollbar_animator;
  int scrollbar_fade_delay_ms;
  int scrollbar_fade_resize_delay_ms;
  int scrollbar_fade_duration_ms;
  float scrollbar_show_scale_threshold;
  SkColor solid_color_scrollbar_color;
  bool timeout_and_draw_when_animation_checkerboards;
  int maximum_number_of_failed_draws_before_draw_is_forced_;
  bool layer_transforms_should_scale_layer_contents;
  bool layers_always_allowed_lcd_text;
  float minimum_contents_scale;
  float low_res_contents_scale_factor;
  float top_controls_show_threshold;
  float top_controls_hide_threshold;
  double background_animation_rate;
  size_t max_partial_texture_updates;
  gfx::Size default_tile_size;
  gfx::Size max_untiled_layer_size;
  gfx::Size default_tile_grid_size;
  gfx::Size minimum_occlusion_tracking_size;
  bool use_pinch_zoom_scrollbars;
  bool use_pinch_virtual_viewport;
  size_t max_tiles_for_interest_area;
  float skewport_target_time_in_seconds;
  int skewport_extrapolation_limit_in_content_pixels;
  size_t max_unused_resource_memory_percentage;
  size_t max_memory_for_prepaint_percentage;
  bool strict_layer_property_change_checking;
  bool use_one_copy;
  bool use_zero_copy;
  bool enable_elastic_overscroll;
  unsigned use_image_texture_target;
  bool ignore_root_layer_flings;
  size_t scheduled_raster_task_limit;
  bool use_occlusion_for_tile_prioritization;
  bool record_full_layer;
  bool use_display_lists;
  bool verify_property_trees;

  LayerTreeDebugState initial_debug_state;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_SETTINGS_H_

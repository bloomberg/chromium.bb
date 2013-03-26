// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_settings.h"

#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"

namespace cc {

LayerTreeSettings::LayerTreeSettings()
    : accelerate_painting(false),
      compositor_frame_message(false),
      impl_side_painting(false),
      render_vsync_enabled(true),
      render_vsync_notification_enabled(false),
      per_tile_painting_enabled(false),
      partial_swap_enabled(false),
      cache_render_pass_contents(true),
      right_aligned_scheduling_enabled(false),
      accelerated_animation_enabled(true),
      background_color_instead_of_checkerboard(false),
      show_overdraw_in_tracing(false),
      can_use_lcd_text(true),
      should_clear_root_render_pass(true),
      use_linear_fade_scrollbar_animator(false),
      solid_color_scrollbars(false),
      solid_color_scrollbar_color(SK_ColorWHITE),
      solid_color_scrollbar_thickness_dip(-1),
      calculate_top_controls_position(false),
      use_cheapness_estimator(false),
      use_color_estimator(false),
      use_memory_management(true),
      prediction_benchmarking(false),
      minimum_contents_scale(0.0625f),
      low_res_contents_scale_factor(0.125f),
      top_controls_height(0.f),
      top_controls_show_threshold(0.5f),
      top_controls_hide_threshold(0.5f),
      refresh_rate(0.0),
      max_partial_texture_updates(std::numeric_limits<size_t>::max()),
      num_raster_threads(1),
      default_tile_size(gfx::Size(256, 256)),
      max_untiled_layer_size(gfx::Size(512, 512)),
      minimum_occlusion_tracking_size(gfx::Size(160, 160)),
      use_pinch_zoom_scrollbars(false) {
  // TODO(danakj): Renable surface caching when we can do it more realiably.
  // crbug.com/170713
  cache_render_pass_contents = false;
}

LayerTreeSettings::~LayerTreeSettings() {}

}  // namespace cc

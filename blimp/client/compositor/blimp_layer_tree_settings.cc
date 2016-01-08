// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/compositor/blimp_layer_tree_settings.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/sys_info.h"
#include "cc/base/switches.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_switches.h"

namespace blimp {
namespace client {

// TODO(dtrainor): This is temporary to get the compositor up and running.
// Much of this will either have to be pulled from the server or refactored to
// share the settings from render_widget_compositor.cc.
void PopulateCommonLayerTreeSettings(cc::LayerTreeSettings* settings) {
  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings->layer_transforms_should_scale_layer_contents = true;

  settings->main_frame_before_activation_enabled = false;
  settings->accelerated_animation_enabled = true;
  settings->default_tile_size = gfx::Size(256, 256);
  settings->gpu_rasterization_msaa_sample_count = 0;
  settings->gpu_rasterization_forced = false;
  settings->gpu_rasterization_enabled = false;
  settings->can_use_lcd_text = false;
  settings->use_distance_field_text = false;
#if defined(OS_MACOSX)
  settings->use_zero_copy = true;
#else
  settings->use_zero_copy = false;
#endif
  settings->enable_elastic_overscroll = false;
  settings->image_decode_tasks_enabled = false;
  settings->verify_property_trees = false;
  settings->single_thread_proxy_scheduler = false;
  settings->initial_debug_state.show_debug_borders = false;
  settings->initial_debug_state.show_fps_counter = false;
  settings->initial_debug_state.show_layer_animation_bounds_rects = false;
  settings->initial_debug_state.show_paint_rects = false;
  settings->initial_debug_state.show_property_changed_rects = false;
  settings->initial_debug_state.show_surface_damage_rects = false;
  settings->initial_debug_state.show_screen_space_rects = false;
  settings->initial_debug_state.show_replica_screen_space_rects = false;
  settings->initial_debug_state.SetRecordRenderingStats(false);
  settings->strict_layer_property_change_checking = false;

#if defined(OS_ANDROID)
  if (base::SysInfo::IsLowEndDevice())
    settings->gpu_rasterization_enabled = false;
  settings->using_synchronous_renderer_compositor = false;
  settings->scrollbar_animator = cc::LayerTreeSettings::LINEAR_FADE;
  settings->scrollbar_fade_delay_ms = 300;
  settings->scrollbar_fade_resize_delay_ms = 2000;
  settings->scrollbar_fade_duration_ms = 300;
  settings->solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  settings->renderer_settings.highp_threshold_min = 2048;
  settings->ignore_root_layer_flings = false;
  bool use_low_memory_policy = base::SysInfo::IsLowEndDevice();
  settings->renderer_settings.use_rgba_4444_textures = use_low_memory_policy;
  if (use_low_memory_policy) {
    // On low-end we want to be very carefull about killing other
    // apps. So initially we use 50% more memory to avoid flickering
    // or raster-on-demand.
    settings->max_memory_for_prepaint_percentage = 67;
  } else {
    // On other devices we have increased memory excessively to avoid
    // raster-on-demand already, so now we reserve 50% _only_ to avoid
    // raster-on-demand, and use 50% of the memory otherwise.
    settings->max_memory_for_prepaint_percentage = 50;
  }
  settings->renderer_settings.should_clear_root_render_pass = true;

  // TODO(danakj): Only do this on low end devices.
  settings->create_low_res_tiling = true;

// TODO(dtrainor): Investigate whether or not we want to use an external
// source here.
// settings->use_external_begin_frame_source = true;

#elif !defined(OS_MACOSX)
  settings->scrollbar_animator = cc::LayerTreeSettings::LINEAR_FADE;
  settings->solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  settings->scrollbar_fade_delay_ms = 500;
  settings->scrollbar_fade_resize_delay_ms = 500;
  settings->scrollbar_fade_duration_ms = 300;

  // When pinching in, only show the pinch-viewport overlay scrollbars if the
  // page scale is at least some threshold away from the minimum. i.e. don't
  // show the pinch scrollbars when at minimum scale.
  // TODO(dtrainor): Update this since https://crrev.com/1267603004 landed.
  // settings->scrollbar_show_scale_threshold = 1.05f;
#endif

  // Blimp always uses new cc::AnimationHost system.
  settings->use_compositor_animation_timelines = true;
}

}  // namespace client
}  // namespace blimp

// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_SETTINGS_H_
#define CC_TREES_LAYER_TREE_SETTINGS_H_

#include <stddef.h>

#include <vector>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/tiles/tile_manager_settings.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/resource_format.h"
#include "components/viz/common/resources/resource_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class CC_EXPORT LayerTreeSettings {
 public:
  LayerTreeSettings();
  LayerTreeSettings(const LayerTreeSettings& other);
  virtual ~LayerTreeSettings();

  SchedulerSettings ToSchedulerSettings() const;
  TileManagerSettings ToTileManagerSettings() const;

  viz::ResourceSettings resource_settings;
  bool single_thread_proxy_scheduler = true;
  bool main_frame_before_activation_enabled = false;
  bool using_synchronous_renderer_compositor = false;
  bool enable_latency_recovery = true;
  bool can_use_lcd_text = true;
  bool use_distance_field_text = false;
  bool gpu_rasterization_forced = false;
  bool async_worker_context_enabled = false;
  int gpu_rasterization_msaa_sample_count = 0;
  float gpu_rasterization_skewport_target_time_in_seconds = 0.2f;
  bool create_low_res_tiling = false;
  bool use_stream_video_draw_quad = false;

  enum ScrollbarAnimator {
    NO_ANIMATOR,
    ANDROID_OVERLAY,
    AURA_OVERLAY,
  };
  ScrollbarAnimator scrollbar_animator = NO_ANIMATOR;
  base::TimeDelta scrollbar_fade_delay;
  base::TimeDelta scrollbar_fade_duration;
  base::TimeDelta scrollbar_thinning_duration;
  SkColor solid_color_scrollbar_color = SK_ColorWHITE;
  bool timeout_and_draw_when_animation_checkerboards = true;
  bool layer_transforms_should_scale_layer_contents = false;
  bool layers_always_allowed_lcd_text = false;
  float minimum_contents_scale = 0.0625f;
  float low_res_contents_scale_factor = 0.25f;
  float top_controls_show_threshold = 0.5f;
  float top_controls_hide_threshold = 0.5f;
  double background_animation_rate = 1.0;
  gfx::Size default_tile_size;
  gfx::Size max_untiled_layer_size;
  gfx::Size minimum_occlusion_tracking_size;
  // 3000 pixels should give sufficient area for prepainting.
  // Note this value is specified with an ideal contents scale in mind. That
  // is, the ideal tiling would use this value as the padding.
  // TODO(vmpstr): Figure out a better number that doesn't depend on scale.
  int tiling_interest_area_padding = 3000;
  float skewport_target_time_in_seconds = 1.0f;
  int skewport_extrapolation_limit_in_screen_pixels = 2000;
  size_t max_memory_for_prepaint_percentage = 100;
  bool use_zero_copy = false;
  bool use_partial_raster = false;
  bool enable_elastic_overscroll = false;
  bool ignore_root_layer_flings = false;
  size_t scheduled_raster_task_limit = 32;
  bool use_occlusion_for_tile_prioritization = false;
  bool use_layer_lists = false;
  int max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;
  ManagedMemoryPolicy gpu_memory_policy;
  ManagedMemoryPolicy software_memory_policy;
  size_t decoded_image_cache_budget_bytes = 128 * 1024 * 1024;
  size_t decoded_image_working_set_budget_bytes = 128 * 1024 * 1024;
  int max_preraster_distance_in_screen_pixels = 1000;
  viz::ResourceFormat preferred_tile_format;

  bool enable_color_correct_rasterization = false;

  // TODO(sunxd): remove this flag when filter demoting and aa of mask layers
  // are implemented.
  bool enable_mask_tiling = true;

  // If set to true, the compositor may selectively defer image decodes to the
  // Image Decode Service and raster tiles without images until the decode is
  // ready.
  bool enable_checker_imaging = false;

  LayerTreeDebugState initial_debug_state;

  // Indicates that the LayerTreeHost should defer commits unless it has a valid
  // viz::LocalSurfaceId set.
  bool enable_surface_synchronization = false;

  // Indicates the case when a sub-frame gets its own LayerTree because it's
  // rendered in a different process from its ancestor frames.
  bool is_layer_tree_for_subframe = false;

  // Determines whether we disallow non-exact matches when finding resources
  // in ResourcePool. Only used for layout or pixel tests, as non-deterministic
  // resource sizes can lead to floating point error and noise in these tests.
  bool disallow_non_exact_resource_reuse = false;

  // Whether the Scheduler should wait for all pipeline stages before attempting
  // to draw. If |true|, they will block indefinitely until all stages have
  // completed the current BeginFrame before triggering their own BeginFrame
  // deadlines.
  bool wait_for_all_pipeline_stages_before_draw = false;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_SETTINGS_H_

// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_SETTINGS_H_
#define CC_TREES_LAYER_TREE_SETTINGS_H_

#include <stddef.h>

#include <vector>

#include "cc/base/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/renderer_settings.h"
#include "cc/scheduler/scheduler_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

namespace proto {
class LayerTreeSettings;
}  // namespace proto

class CC_EXPORT LayerTreeSettings {
 public:
  LayerTreeSettings();
  LayerTreeSettings(const LayerTreeSettings& other);
  virtual ~LayerTreeSettings();

  bool operator==(const LayerTreeSettings& other) const;

  void ToProtobuf(proto::LayerTreeSettings* proto) const;
  void FromProtobuf(const proto::LayerTreeSettings& proto);

  SchedulerSettings ToSchedulerSettings() const;

  RendererSettings renderer_settings;
  bool single_thread_proxy_scheduler;
  // TODO(enne): Remove this after everything uses output surface begin frames.
  bool use_external_begin_frame_source;
  // TODO(enne): Temporary staging for unified begin frame source work.
  bool use_output_surface_begin_frame_source;
  bool main_frame_before_activation_enabled;
  bool using_synchronous_renderer_compositor;
  bool can_use_lcd_text;
  bool use_distance_field_text;
  bool gpu_rasterization_enabled;
  bool gpu_rasterization_forced;
  bool async_worker_context_enabled;
  int gpu_rasterization_msaa_sample_count;
  float gpu_rasterization_skewport_target_time_in_seconds;
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
  SkColor solid_color_scrollbar_color;
  bool timeout_and_draw_when_animation_checkerboards;
  bool layer_transforms_should_scale_layer_contents;
  bool layers_always_allowed_lcd_text;
  float minimum_contents_scale;
  float low_res_contents_scale_factor;
  float top_controls_show_threshold;
  float top_controls_hide_threshold;
  double background_animation_rate;
  gfx::Size default_tile_size;
  gfx::Size max_untiled_layer_size;
  gfx::Size minimum_occlusion_tracking_size;
  int tiling_interest_area_padding;
  float skewport_target_time_in_seconds;
  int skewport_extrapolation_limit_in_screen_pixels;
  size_t max_memory_for_prepaint_percentage;
  bool use_zero_copy;
  bool use_partial_raster;
  bool enable_elastic_overscroll;
  // An array of image texture targets for each GpuMemoryBuffer format.
  std::vector<unsigned> use_image_texture_targets;
  bool ignore_root_layer_flings;
  size_t scheduled_raster_task_limit;
  bool use_occlusion_for_tile_prioritization;
  bool image_decode_tasks_enabled;
  bool wait_for_beginframe_interval;
  bool abort_commit_before_output_surface_creation;
  bool use_mouse_wheel_gestures;
  bool use_layer_lists;
  int max_staging_buffer_usage_in_bytes;
  ManagedMemoryPolicy memory_policy_;

  // If set to true, the display item list will internally cache a SkPicture for
  // raster rather than directly using the display items.
  bool use_cached_picture_raster;

  LayerTreeDebugState initial_debug_state;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_SETTINGS_H_

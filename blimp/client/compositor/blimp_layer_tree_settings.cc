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
#include "content/public/common/content_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_switches.h"

namespace {

bool GetSwitchValueAsInt(const base::CommandLine& command_line,
                         const std::string& switch_string,
                         int min_value,
                         int max_value,
                         int* result) {
  std::string string_value = command_line.GetSwitchValueASCII(switch_string);
  int int_value;
  if (base::StringToInt(string_value, &int_value) && int_value >= min_value &&
      int_value <= max_value) {
    *result = int_value;
    return true;
  } else {
    return false;
  }
}

void StringToUintVector(const std::string& str, std::vector<unsigned>* vector) {
  DCHECK(vector->empty());
  std::vector<std::string> pieces =
      base::SplitString(str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK_EQ(pieces.size(), static_cast<size_t>(gfx::BufferFormat::LAST) + 1);
  for (size_t i = 0; i < pieces.size(); ++i) {
    unsigned number = 0;
    bool succeed = base::StringToUint(pieces[i], &number);
    DCHECK(succeed);
    vector->push_back(number);
  }
}

}  // namespace

namespace blimp {

// TODO(dtrainor): This is temporary to get the compositor up and running.
// Much of this will either have to be pulled from the server or refactored to
// share the settings from render_widget_compositor.cc.
void PopulateCommonLayerTreeSettings(cc::LayerTreeSettings* settings) {
  const base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings->layer_transforms_should_scale_layer_contents = true;

  if (cmd.HasSwitch(switches::kDisableGpuVsync)) {
    std::string display_vsync_string =
        cmd.GetSwitchValueASCII(switches::kDisableGpuVsync);
    if (display_vsync_string == "gpu") {
      settings->renderer_settings.disable_display_vsync = true;
    } else if (display_vsync_string == "beginframe") {
      settings->wait_for_beginframe_interval = false;
    } else {
      settings->renderer_settings.disable_display_vsync = true;
      settings->wait_for_beginframe_interval = false;
    }
  }
  settings->main_frame_before_activation_enabled =
      cmd.HasSwitch(cc::switches::kEnableMainFrameBeforeActivation) &&
      !cmd.HasSwitch(cc::switches::kDisableMainFrameBeforeActivation);
  settings->accelerated_animation_enabled =
      !cmd.HasSwitch(cc::switches::kDisableThreadedAnimation);

  settings->default_tile_size = gfx::Size(256, 256);
  if (cmd.HasSwitch(switches::kDefaultTileWidth)) {
    int tile_width = 0;
    GetSwitchValueAsInt(cmd, switches::kDefaultTileWidth, 1,
                        std::numeric_limits<int>::max(), &tile_width);
    settings->default_tile_size.set_width(tile_width);
  }
  if (cmd.HasSwitch(switches::kDefaultTileHeight)) {
    int tile_height = 0;
    GetSwitchValueAsInt(cmd, switches::kDefaultTileHeight, 1,
                        std::numeric_limits<int>::max(), &tile_height);
    settings->default_tile_size.set_height(tile_height);
  }

  int max_untiled_layer_width = settings->max_untiled_layer_size.width();
  if (cmd.HasSwitch(switches::kMaxUntiledLayerWidth)) {
    GetSwitchValueAsInt(cmd, switches::kMaxUntiledLayerWidth, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_width);
  }
  int max_untiled_layer_height = settings->max_untiled_layer_size.height();
  if (cmd.HasSwitch(switches::kMaxUntiledLayerHeight)) {
    GetSwitchValueAsInt(cmd, switches::kMaxUntiledLayerHeight, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_height);
  }

  settings->max_untiled_layer_size =
      gfx::Size(max_untiled_layer_width, max_untiled_layer_height);

  settings->gpu_rasterization_msaa_sample_count = 0;
  if (cmd.HasSwitch(switches::kGpuRasterizationMSAASampleCount)) {
    GetSwitchValueAsInt(cmd, switches::kGpuRasterizationMSAASampleCount, 0,
                        std::numeric_limits<int>::max(),
                        &settings->gpu_rasterization_msaa_sample_count);
  }

  settings->gpu_rasterization_forced =
      cmd.HasSwitch(switches::kForceGpuRasterization);
  settings->gpu_rasterization_enabled =
      cmd.HasSwitch(switches::kEnableGpuRasterization);

  settings->can_use_lcd_text = false;
  settings->use_distance_field_text =
      cmd.HasSwitch(switches::kEnableDistanceFieldText);

#if defined(OS_MACOSX)
  settings->use_zero_copy = !cmd.HasSwitch(switches::kDisableZeroCopy);
#else
  settings->use_zero_copy = cmd.HasSwitch(switches::kEnableZeroCopy);
#endif

  settings->enable_elastic_overscroll = false;

  if (cmd.HasSwitch(switches::kContentImageTextureTarget)) {
    settings->use_image_texture_targets.clear();
    StringToUintVector(
        cmd.GetSwitchValueASCII(switches::kContentImageTextureTarget),
        &settings->use_image_texture_targets);
  }

  settings->image_decode_tasks_enabled = false;
  if (cmd.HasSwitch(switches::kNumRasterThreads)) {
    int num_raster_threads = 0;
    GetSwitchValueAsInt(cmd, switches::kNumRasterThreads, 0,
                        std::numeric_limits<int>::max(), &num_raster_threads);
    settings->image_decode_tasks_enabled = num_raster_threads > 1;
  }

  if (cmd.HasSwitch(cc::switches::kTopControlsShowThreshold)) {
    std::string top_threshold_str =
        cmd.GetSwitchValueASCII(cc::switches::kTopControlsShowThreshold);
    double show_threshold;
    if (base::StringToDouble(top_threshold_str, &show_threshold) &&
        show_threshold >= 0.f && show_threshold <= 1.f)
      settings->top_controls_show_threshold = show_threshold;
  }

  if (cmd.HasSwitch(cc::switches::kTopControlsHideThreshold)) {
    std::string top_threshold_str =
        cmd.GetSwitchValueASCII(cc::switches::kTopControlsHideThreshold);
    double hide_threshold;
    if (base::StringToDouble(top_threshold_str, &hide_threshold) &&
        hide_threshold >= 0.f && hide_threshold <= 1.f)
      settings->top_controls_hide_threshold = hide_threshold;
  }

  settings->verify_property_trees =
      cmd.HasSwitch(cc::switches::kEnablePropertyTreeVerification);
  settings->renderer_settings.allow_antialiasing &=
      !cmd.HasSwitch(cc::switches::kDisableCompositedAntialiasing);
  settings->single_thread_proxy_scheduler = false;

  // These flags should be mirrored by UI versions in ui/compositor/.
  settings->initial_debug_state.show_debug_borders =
      cmd.HasSwitch(cc::switches::kShowCompositedLayerBorders);
  settings->initial_debug_state.show_fps_counter =
      cmd.HasSwitch(cc::switches::kShowFPSCounter);
  settings->initial_debug_state.show_layer_animation_bounds_rects =
      cmd.HasSwitch(cc::switches::kShowLayerAnimationBounds);
  settings->initial_debug_state.show_paint_rects =
      cmd.HasSwitch(switches::kShowPaintRects);
  settings->initial_debug_state.show_property_changed_rects =
      cmd.HasSwitch(cc::switches::kShowPropertyChangedRects);
  settings->initial_debug_state.show_surface_damage_rects =
      cmd.HasSwitch(cc::switches::kShowSurfaceDamageRects);
  settings->initial_debug_state.show_screen_space_rects =
      cmd.HasSwitch(cc::switches::kShowScreenSpaceRects);
  settings->initial_debug_state.show_replica_screen_space_rects =
      cmd.HasSwitch(cc::switches::kShowReplicaScreenSpaceRects);

  settings->initial_debug_state.SetRecordRenderingStats(
      cmd.HasSwitch(cc::switches::kEnableGpuBenchmarking));

  if (cmd.HasSwitch(cc::switches::kSlowDownRasterScaleFactor)) {
    const int kMinSlowDownScaleFactor = 0;
    const int kMaxSlowDownScaleFactor = INT_MAX;
    GetSwitchValueAsInt(
        cmd, cc::switches::kSlowDownRasterScaleFactor, kMinSlowDownScaleFactor,
        kMaxSlowDownScaleFactor,
        &settings->initial_debug_state.slow_down_raster_scale_factor);
  }

  settings->strict_layer_property_change_checking =
      cmd.HasSwitch(cc::switches::kStrictLayerPropertyChangeChecking);

#if defined(OS_ANDROID)
  if (base::SysInfo::IsLowEndDevice())
    settings->gpu_rasterization_enabled = false;
  settings->using_synchronous_renderer_compositor = false;
  settings->record_full_layer = false;
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
  if (ui::IsOverlayScrollbarEnabled()) {
    settings->scrollbar_animator = cc::LayerTreeSettings::THINNING;
    settings->solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  } else {
    settings->scrollbar_animator = cc::LayerTreeSettings::LINEAR_FADE;
    settings->solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  }
  settings->scrollbar_fade_delay_ms = 500;
  settings->scrollbar_fade_resize_delay_ms = 500;
  settings->scrollbar_fade_duration_ms = 300;

  // When pinching in, only show the pinch-viewport overlay scrollbars if the
  // page scale is at least some threshold away from the minimum. i.e. don't
  // show the pinch scrollbars when at minimum scale.
  // TODO(dtrainor): Update this since https://crrev.com/1267603004 landed.
  // settings->scrollbar_show_scale_threshold = 1.05f;
#endif

  if (cmd.HasSwitch(switches::kEnableLowResTiling))
    settings->create_low_res_tiling = true;
  if (cmd.HasSwitch(switches::kDisableLowResTiling))
    settings->create_low_res_tiling = false;
  if (cmd.HasSwitch(cc::switches::kEnableBeginFrameScheduling))
    settings->use_external_begin_frame_source = true;
}

}  // namespace blimp

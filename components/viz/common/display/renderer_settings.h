// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DISPLAY_RENDERER_SETTINGS_H_
#define COMPONENTS_VIZ_COMMON_DISPLAY_RENDERER_SETTINGS_H_

#include <stddef.h>

#include "components/viz/common/resources/resource_settings.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

class VIZ_COMMON_EXPORT RendererSettings {
 public:
  RendererSettings();
  RendererSettings(const RendererSettings& other);
  ~RendererSettings();

  ResourceSettings resource_settings;
  bool allow_antialiasing = true;
  bool force_antialiasing = false;
  bool force_blending_with_shaders = false;
  bool partial_swap_enabled = false;
  bool finish_rendering_on_resize = false;
  bool should_clear_root_render_pass = true;
  bool release_overlay_resources_after_gpu_query = false;
  bool gl_composited_overlay_candidate_quad_border = false;
  bool show_overdraw_feedback = false;
  bool enable_draw_occlusion = false;
  bool use_skia_renderer = false;
  int highp_threshold_min = 0;

  // Determines whether we disallow non-exact matches when finding resources
  // in ResourcePool. Only used for layout or pixel tests, as non-deterministic
  // resource sizes can lead to floating point error and noise in these tests.
  bool disallow_non_exact_resource_reuse = false;

  int slow_down_compositing_scale_factor = 1;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DISPLAY_RENDERER_SETTINGS_H_

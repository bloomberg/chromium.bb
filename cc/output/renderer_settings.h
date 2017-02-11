// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_RENDERER_SETTINGS_H_
#define CC_OUTPUT_RENDERER_SETTINGS_H_

#include <stddef.h>

#include "cc/base/cc_export.h"
#include "cc/output/buffer_to_texture_target_map.h"
#include "cc/resources/resource_format.h"

namespace cc {

class CC_EXPORT RendererSettings {
 public:
  RendererSettings();
  RendererSettings(const RendererSettings& other);
  ~RendererSettings();

  bool allow_antialiasing = true;
  bool force_antialiasing = false;
  bool force_blending_with_shaders = false;
  bool partial_swap_enabled = false;
  bool finish_rendering_on_resize = false;
  bool should_clear_root_render_pass = true;
  bool disable_display_vsync = false;
  bool release_overlay_resources_after_gpu_query = false;
  bool gl_composited_texture_quad_border = false;
  bool show_overdraw_feedback = false;
  bool enable_color_correct_rendering = false;

  double refresh_rate = 60.0;
  int highp_threshold_min = 0;
  size_t texture_id_allocation_chunk_size = 64;
  bool use_gpu_memory_buffer_resources = false;
  ResourceFormat preferred_tile_format;
  BufferToTextureTargetMap buffer_to_texture_target_map;

  bool operator==(const RendererSettings& other) const;
};

}  // namespace cc

#endif  // CC_OUTPUT_RENDERER_SETTINGS_H_

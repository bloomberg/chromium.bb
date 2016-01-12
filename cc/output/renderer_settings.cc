// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/renderer_settings.h"

#include <limits>

#include "base/logging.h"
#include "cc/proto/renderer_settings.pb.h"

namespace cc {

RendererSettings::RendererSettings()
    : allow_antialiasing(true),
      force_antialiasing(false),
      force_blending_with_shaders(false),
      partial_swap_enabled(false),
      finish_rendering_on_resize(false),
      should_clear_root_render_pass(true),
      disable_display_vsync(false),
      release_overlay_resources_on_swap_complete(false),
      refresh_rate(60.0),
      highp_threshold_min(0),
      use_rgba_4444_textures(false),
      texture_id_allocation_chunk_size(64),
      use_gpu_memory_buffer_resources(false) {}

RendererSettings::~RendererSettings() {
}

void RendererSettings::ToProtobuf(proto::RendererSettings* proto) const {
  proto->set_allow_antialiasing(allow_antialiasing);
  proto->set_force_antialiasing(force_antialiasing);
  proto->set_force_blending_with_shaders(force_blending_with_shaders);
  proto->set_partial_swap_enabled(partial_swap_enabled);
  proto->set_finish_rendering_on_resize(finish_rendering_on_resize);
  proto->set_should_clear_root_render_pass(should_clear_root_render_pass);
  proto->set_disable_display_vsync(disable_display_vsync);
  proto->set_release_overlay_resources_on_swap_complete(
      release_overlay_resources_on_swap_complete);
  proto->set_refresh_rate(refresh_rate);
  proto->set_highp_threshold_min(highp_threshold_min);
  proto->set_use_rgba_4444_textures(use_rgba_4444_textures);
  proto->set_texture_id_allocation_chunk_size(texture_id_allocation_chunk_size);
  proto->set_use_gpu_memory_buffer_resources(use_gpu_memory_buffer_resources);
}

void RendererSettings::FromProtobuf(const proto::RendererSettings& proto) {
  allow_antialiasing = proto.allow_antialiasing();
  force_antialiasing = proto.force_antialiasing();
  force_blending_with_shaders = proto.force_blending_with_shaders();
  partial_swap_enabled = proto.partial_swap_enabled();
  finish_rendering_on_resize = proto.finish_rendering_on_resize();
  should_clear_root_render_pass = proto.should_clear_root_render_pass();
  disable_display_vsync = proto.disable_display_vsync();
  release_overlay_resources_on_swap_complete =
      proto.release_overlay_resources_on_swap_complete();
  refresh_rate = proto.refresh_rate();
  highp_threshold_min = proto.highp_threshold_min();
  use_rgba_4444_textures = proto.use_rgba_4444_textures();
  texture_id_allocation_chunk_size = proto.texture_id_allocation_chunk_size();
  use_gpu_memory_buffer_resources = proto.use_gpu_memory_buffer_resources();
}

bool RendererSettings::operator==(const RendererSettings& other) const {
  return allow_antialiasing == other.allow_antialiasing &&
         force_antialiasing == other.force_antialiasing &&
         force_blending_with_shaders == other.force_blending_with_shaders &&
         partial_swap_enabled == other.partial_swap_enabled &&
         finish_rendering_on_resize == other.finish_rendering_on_resize &&
         should_clear_root_render_pass == other.should_clear_root_render_pass &&
         disable_display_vsync == other.disable_display_vsync &&
         release_overlay_resources_on_swap_complete ==
             other.release_overlay_resources_on_swap_complete &&
         refresh_rate == other.refresh_rate &&
         highp_threshold_min == other.highp_threshold_min &&
         use_rgba_4444_textures == other.use_rgba_4444_textures &&
         texture_id_allocation_chunk_size ==
             other.texture_id_allocation_chunk_size &&
         use_gpu_memory_buffer_resources ==
             other.use_gpu_memory_buffer_resources;
}

}  // namespace cc

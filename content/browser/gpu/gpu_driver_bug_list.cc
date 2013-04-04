// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_driver_bug_list.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "gpu/command_buffer/service/gpu_driver_bug_workaround_type.h"

namespace content {

namespace {

struct DriverBugInfo {
  int feature_type;
  std::string feature_name;
};

}  // namespace anonymous

GpuDriverBugList::GpuDriverBugList()
    : GpuControlList() {
}

GpuDriverBugList::~GpuDriverBugList() {
}

// static
GpuDriverBugList* GpuDriverBugList::Create() {
  GpuDriverBugList* list = new GpuDriverBugList();

  const DriverBugInfo kFeatureList[] = {
      { gpu::CLEAR_ALPHA_IN_READPIXELS, "clear_alpha_in_readpixels" },
      { gpu::CLEAR_UNIFORMS_BEFORE_PROGRAM_USE,
        "clear_uniforms_before_program_use" },
      { gpu::DELETE_INSTEAD_OF_RESIZE_FBO, "delete_instead_of_resize_fbo" },
      { gpu::DISABLE_ANGLE_FRAMEBUFFER_MULTISAMPLE,
        "disable_angle_framebuffer_multisample" },
      { gpu::DISABLE_DEPTH_TEXTURE, "disable_depth_texture" },
      { gpu::DISABLE_EXT_OCCLUSION_QUERY, "disable_ext_occlusion_query" },
      { gpu::ENABLE_CHROMIUM_FAST_NPOT_MO8_TEXTURES,
        "enable_chromium_fast_npot_mo8_textures" },
      { gpu::EXIT_ON_CONTEXT_LOST, "exit_on_context_lost" },
      { gpu::FLUSH_ON_CONTEXT_SWITCH, "flush_on_context_switch" },
      { gpu::MAX_CUBE_MAP_TEXTURE_SIZE_LIMIT_1024,
        "max_cube_map_texture_size_limit_1024" },
      { gpu::MAX_CUBE_MAP_TEXTURE_SIZE_LIMIT_4096,
        "max_cube_map_texture_size_limit_4096" },
      { gpu::MAX_CUBE_MAP_TEXTURE_SIZE_LIMIT_512,
        "max_cube_map_texture_size_limit_512" },
      { gpu::MAX_TEXTURE_SIZE_LIMIT_4096, "max_texture_size_limit_4096" },
      { gpu::NEEDS_GLSL_BUILT_IN_FUNCTION_EMULATION,
        "needs_glsl_built_in_function_emulation" },
      { gpu::NEEDS_OFFSCREEN_BUFFER_WORKAROUND,
        "needs_offscreen_buffer_workaround" },
      { gpu::RESTORE_SCISSOR_ON_FBO_CHANGE, "restore_scissor_on_fbo_change" },
      { gpu::REVERSE_POINT_SPRITE_COORD_ORIGIN,
        "reverse_point_sprite_coord_origin" },
      { gpu::SET_TEXTURE_FILTER_BEFORE_GENERATING_MIPMAP,
        "set_texture_filter_before_generating_mipmap" },
      { gpu::USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS,
        "use_client_side_arrays_for_stream_buffers" },
      { gpu::USE_CURRENT_PROGRAM_AFTER_SUCCESSFUL_LINK,
        "use_current_program_after_successful_link" }
  };
  DCHECK_EQ(static_cast<int>(arraysize(kFeatureList)),
            gpu::NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES);
  for (int i = 0; i < gpu::NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES; ++i) {
    list->AddSupportedFeature(kFeatureList[i].feature_name,
                              kFeatureList[i].feature_type);
  }
  return list;
}

}  // namespace content


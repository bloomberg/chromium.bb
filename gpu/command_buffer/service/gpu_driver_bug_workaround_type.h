// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_DRIVER_BUG_WORDAROUND_TYPE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_DRIVER_BUG_WORDAROUND_TYPE_H_

#include "gpu/gpu_export.h"

#define GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)            \
  GPU_OP(CLEAR_ALPHA_IN_READPIXELS,                   \
         clear_alpha_in_readpixels)                   \
  GPU_OP(CLEAR_UNIFORMS_BEFORE_PROGRAM_USE,           \
         clear_uniforms_before_program_use)           \
  GPU_OP(DELETE_INSTEAD_OF_RESIZE_FBO,                \
         delete_instead_of_resize_fbo)                \
  GPU_OP(DISABLE_ANGLE_FRAMEBUFFER_MULTISAMPLE,       \
         disable_angle_framebuffer_multisample)       \
  GPU_OP(DISABLE_DEPTH_TEXTURE,                       \
         disable_depth_texture)                       \
  GPU_OP(DISABLE_EXT_DRAW_BUFFERS,                    \
         disable_ext_draw_buffers)                    \
  GPU_OP(DISABLE_EXT_OCCLUSION_QUERY,                 \
         disable_ext_occlusion_query)                 \
  GPU_OP(ENABLE_CHROMIUM_FAST_NPOT_MO8_TEXTURES,      \
         enable_chromium_fast_npot_mo8_textures)      \
  GPU_OP(EXIT_ON_CONTEXT_LOST,                        \
         exit_on_context_lost)                        \
  GPU_OP(FLUSH_ON_CONTEXT_SWITCH,                     \
         flush_on_context_switch)                     \
  GPU_OP(UNBIND_FBO_ON_CONTEXT_SWITCH,                \
         unbind_fbo_on_context_switch)                \
  GPU_OP(MAX_CUBE_MAP_TEXTURE_SIZE_LIMIT_1024,        \
         max_cube_map_texture_size_limit_1024)        \
  GPU_OP(MAX_CUBE_MAP_TEXTURE_SIZE_LIMIT_4096,        \
         max_cube_map_texture_size_limit_4096)        \
  GPU_OP(MAX_CUBE_MAP_TEXTURE_SIZE_LIMIT_512,         \
         max_cube_map_texture_size_limit_512)         \
  GPU_OP(MAX_TEXTURE_SIZE_LIMIT_4096,                 \
         max_texture_size_limit_4096)                 \
  GPU_OP(NEEDS_GLSL_BUILT_IN_FUNCTION_EMULATION,      \
         needs_glsl_built_in_function_emulation)      \
  GPU_OP(NEEDS_OFFSCREEN_BUFFER_WORKAROUND,           \
         needs_offscreen_buffer_workaround)           \
  GPU_OP(RESTORE_SCISSOR_ON_FBO_CHANGE,               \
         restore_scissor_on_fbo_change)               \
  GPU_OP(REVERSE_POINT_SPRITE_COORD_ORIGIN,           \
         reverse_point_sprite_coord_origin)           \
  GPU_OP(SET_TEXTURE_FILTER_BEFORE_GENERATING_MIPMAP, \
         set_texture_filter_before_generating_mipmap) \
  GPU_OP(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS,   \
         use_client_side_arrays_for_stream_buffers)   \
  GPU_OP(USE_CURRENT_PROGRAM_AFTER_SUCCESSFUL_LINK,   \
         use_current_program_after_successful_link)

namespace gpu {

// Provides all types of GPU driver bug workarounds.
enum GPU_EXPORT GpuDriverBugWorkaroundType {
#define GPU_OP(type, name) type,
  GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
  NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_DRIVER_BUG_WORDAROUND_TYPE_H_


// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here.
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/gpu_export.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

IPC_STRUCT_TRAITS_BEGIN(gpu::Capabilities::ShaderPrecision)
  IPC_STRUCT_TRAITS_MEMBER(min_range)
  IPC_STRUCT_TRAITS_MEMBER(max_range)
  IPC_STRUCT_TRAITS_MEMBER(precision)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::Capabilities::PerStagePrecisions)
  IPC_STRUCT_TRAITS_MEMBER(low_int)
  IPC_STRUCT_TRAITS_MEMBER(medium_int)
  IPC_STRUCT_TRAITS_MEMBER(high_int)
  IPC_STRUCT_TRAITS_MEMBER(low_float)
  IPC_STRUCT_TRAITS_MEMBER(medium_float)
  IPC_STRUCT_TRAITS_MEMBER(high_float)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::Capabilities)
  IPC_STRUCT_TRAITS_MEMBER(vertex_shader_precisions)
  IPC_STRUCT_TRAITS_MEMBER(fragment_shader_precisions)
  IPC_STRUCT_TRAITS_MEMBER(max_combined_texture_image_units)
  IPC_STRUCT_TRAITS_MEMBER(max_cube_map_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(max_fragment_uniform_vectors)
  IPC_STRUCT_TRAITS_MEMBER(max_renderbuffer_size)
  IPC_STRUCT_TRAITS_MEMBER(max_texture_image_units)
  IPC_STRUCT_TRAITS_MEMBER(max_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(max_varying_vectors)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_attribs)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_texture_image_units)
  IPC_STRUCT_TRAITS_MEMBER(max_vertex_uniform_vectors)
  IPC_STRUCT_TRAITS_MEMBER(num_compressed_texture_formats)
  IPC_STRUCT_TRAITS_MEMBER(num_shader_binary_formats)
  IPC_STRUCT_TRAITS_MEMBER(bind_generates_resource_chromium)
  IPC_STRUCT_TRAITS_MEMBER(max_transform_feedback_separate_attribs)
  IPC_STRUCT_TRAITS_MEMBER(max_uniform_buffer_bindings)
  IPC_STRUCT_TRAITS_MEMBER(uniform_buffer_offset_alignment)
  IPC_STRUCT_TRAITS_MEMBER(post_sub_buffer)
  IPC_STRUCT_TRAITS_MEMBER(egl_image_external)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_atc)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_bgra8888)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_dxt1)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_dxt5)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_etc1)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_etc1_npot)
  IPC_STRUCT_TRAITS_MEMBER(texture_rectangle)
  IPC_STRUCT_TRAITS_MEMBER(iosurface)
  IPC_STRUCT_TRAITS_MEMBER(texture_usage)
  IPC_STRUCT_TRAITS_MEMBER(texture_storage)
  IPC_STRUCT_TRAITS_MEMBER(discard_framebuffer)
  IPC_STRUCT_TRAITS_MEMBER(sync_query)
  IPC_STRUCT_TRAITS_MEMBER(image)
  IPC_STRUCT_TRAITS_MEMBER(blend_equation_advanced)
  IPC_STRUCT_TRAITS_MEMBER(blend_equation_advanced_coherent)
  IPC_STRUCT_TRAITS_MEMBER(texture_rg)
IPC_STRUCT_TRAITS_END()

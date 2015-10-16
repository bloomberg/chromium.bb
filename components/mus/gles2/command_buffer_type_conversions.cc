// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_type_conversions.h"

#include "components/mus/public/interfaces/command_buffer.mojom.h"

using mus::mojom::CommandBufferPtr;
using mus::mojom::CommandBufferState;
using mus::mojom::CommandBufferStatePtr;
using mus::mojom::GpuCapabilities;
using mus::mojom::GpuCapabilitiesPtr;
using mus::mojom::GpuInfo;
using mus::mojom::GpuInfoPtr;
using mus::mojom::GpuPerStagePrecisions;
using mus::mojom::GpuPerStagePrecisionsPtr;
using mus::mojom::GpuShaderPrecision;
using mus::mojom::GpuShaderPrecisionPtr;

namespace mojo {

CommandBufferStatePtr
TypeConverter<CommandBufferStatePtr, gpu::CommandBuffer::State>::Convert(
    const gpu::CommandBuffer::State& input) {
  CommandBufferStatePtr result(CommandBufferState::New());
  result->get_offset = input.get_offset;
  result->token = input.token;
  result->error = input.error;
  result->context_lost_reason = input.context_lost_reason;
  result->generation = input.generation;
  return result.Pass();
}

gpu::CommandBuffer::State
TypeConverter<gpu::CommandBuffer::State, CommandBufferStatePtr>::Convert(
    const CommandBufferStatePtr& input) {
  gpu::CommandBuffer::State state;
  state.get_offset = input->get_offset;
  state.token = input->token;
  state.error = static_cast<gpu::error::Error>(input->error);
  state.context_lost_reason =
      static_cast<gpu::error::ContextLostReason>(input->context_lost_reason);
  state.generation = input->generation;
  return state;
}

GpuShaderPrecisionPtr
TypeConverter<GpuShaderPrecisionPtr, gpu::Capabilities::ShaderPrecision>::
    Convert(const gpu::Capabilities::ShaderPrecision& input) {
  GpuShaderPrecisionPtr result(GpuShaderPrecision::New());
  result->min_range = input.min_range;
  result->max_range = input.max_range;
  result->precision = input.precision;
  return result.Pass();
}

gpu::Capabilities::ShaderPrecision TypeConverter<
    gpu::Capabilities::ShaderPrecision,
    GpuShaderPrecisionPtr>::Convert(const GpuShaderPrecisionPtr& input) {
  gpu::Capabilities::ShaderPrecision result;
  result.min_range = input->min_range;
  result.max_range = input->max_range;
  result.precision = input->precision;
  return result;
}

GpuPerStagePrecisionsPtr
TypeConverter<GpuPerStagePrecisionsPtr, gpu::Capabilities::PerStagePrecisions>::
    Convert(const gpu::Capabilities::PerStagePrecisions& input) {
  GpuPerStagePrecisionsPtr result(GpuPerStagePrecisions::New());
  result->low_int = GpuShaderPrecision::From(input.low_int);
  result->medium_int = GpuShaderPrecision::From(input.medium_int);
  result->high_int = GpuShaderPrecision::From(input.high_int);
  result->low_float = GpuShaderPrecision::From(input.low_float);
  result->medium_float = GpuShaderPrecision::From(input.medium_float);
  result->high_float = GpuShaderPrecision::From(input.high_float);
  return result.Pass();
}

gpu::Capabilities::PerStagePrecisions TypeConverter<
    gpu::Capabilities::PerStagePrecisions,
    GpuPerStagePrecisionsPtr>::Convert(const GpuPerStagePrecisionsPtr& input) {
  gpu::Capabilities::PerStagePrecisions result;
  result.low_int = input->low_int.To<gpu::Capabilities::ShaderPrecision>();
  result.medium_int =
      input->medium_int.To<gpu::Capabilities::ShaderPrecision>();
  result.high_int = input->high_int.To<gpu::Capabilities::ShaderPrecision>();
  result.low_float = input->low_float.To<gpu::Capabilities::ShaderPrecision>();
  result.medium_float =
      input->medium_float.To<gpu::Capabilities::ShaderPrecision>();
  result.high_float =
      input->high_float.To<gpu::Capabilities::ShaderPrecision>();
  return result;
}

GpuCapabilitiesPtr
TypeConverter<GpuCapabilitiesPtr, gpu::Capabilities>::Convert(
    const gpu::Capabilities& input) {
  GpuCapabilitiesPtr result(GpuCapabilities::New());
  result->vertex_shader_precisions =
      GpuPerStagePrecisions::From(input.vertex_shader_precisions);
  result->fragment_shader_precisions =
      GpuPerStagePrecisions::From(input.fragment_shader_precisions);
  result->max_combined_texture_image_units =
      input.max_combined_texture_image_units;
  result->max_cube_map_texture_size = input.max_cube_map_texture_size;
  result->max_fragment_uniform_vectors = input.max_fragment_uniform_vectors;
  result->max_renderbuffer_size = input.max_renderbuffer_size;
  result->max_texture_image_units = input.max_texture_image_units;
  result->max_texture_size = input.max_texture_size;
  result->max_varying_vectors = input.max_varying_vectors;
  result->max_vertex_attribs = input.max_vertex_attribs;
  result->max_vertex_texture_image_units = input.max_vertex_texture_image_units;
  result->max_vertex_uniform_vectors = input.max_vertex_uniform_vectors;
  result->num_compressed_texture_formats = input.num_compressed_texture_formats;
  result->num_shader_binary_formats = input.num_shader_binary_formats;
  result->bind_generates_resource_chromium =
      input.bind_generates_resource_chromium;
  result->post_sub_buffer = input.post_sub_buffer;
  result->egl_image_external = input.egl_image_external;
  result->texture_format_bgra8888 = input.texture_format_bgra8888;
  result->texture_format_etc1 = input.texture_format_etc1;
  result->texture_format_etc1_npot = input.texture_format_etc1_npot;
  result->texture_rectangle = input.texture_rectangle;
  result->iosurface = input.iosurface;
  result->texture_usage = input.texture_usage;
  result->texture_storage = input.texture_storage;
  result->discard_framebuffer = input.discard_framebuffer;
  result->sync_query = input.sync_query;
  result->image = input.image;
  result->future_sync_points = input.future_sync_points;
  result->blend_equation_advanced = input.blend_equation_advanced;
  result->blend_equation_advanced_coherent =
      input.blend_equation_advanced_coherent;
  return result.Pass();
}

gpu::Capabilities TypeConverter<gpu::Capabilities, GpuCapabilitiesPtr>::Convert(
    const GpuCapabilitiesPtr& input) {
  gpu::Capabilities result;
  result.vertex_shader_precisions =
      input->vertex_shader_precisions
          .To<gpu::Capabilities::PerStagePrecisions>();
  result.fragment_shader_precisions =
      input->fragment_shader_precisions
          .To<gpu::Capabilities::PerStagePrecisions>();
  result.max_combined_texture_image_units =
      input->max_combined_texture_image_units;
  result.max_cube_map_texture_size = input->max_cube_map_texture_size;
  result.max_fragment_uniform_vectors = input->max_fragment_uniform_vectors;
  result.max_renderbuffer_size = input->max_renderbuffer_size;
  result.max_texture_image_units = input->max_texture_image_units;
  result.max_texture_size = input->max_texture_size;
  result.max_varying_vectors = input->max_varying_vectors;
  result.max_vertex_attribs = input->max_vertex_attribs;
  result.max_vertex_texture_image_units = input->max_vertex_texture_image_units;
  result.max_vertex_uniform_vectors = input->max_vertex_uniform_vectors;
  result.num_compressed_texture_formats = input->num_compressed_texture_formats;
  result.num_shader_binary_formats = input->num_shader_binary_formats;
  result.bind_generates_resource_chromium =
      input->bind_generates_resource_chromium;
  result.post_sub_buffer = input->post_sub_buffer;
  result.egl_image_external = input->egl_image_external;
  result.texture_format_bgra8888 = input->texture_format_bgra8888;
  result.texture_format_etc1 = input->texture_format_etc1;
  result.texture_format_etc1_npot = input->texture_format_etc1_npot;
  result.texture_rectangle = input->texture_rectangle;
  result.iosurface = input->iosurface;
  result.texture_usage = input->texture_usage;
  result.texture_storage = input->texture_storage;
  result.discard_framebuffer = input->discard_framebuffer;
  result.sync_query = input->sync_query;
  result.image = input->image;
  result.future_sync_points = input->future_sync_points;
  result.blend_equation_advanced = input->blend_equation_advanced;
  result.blend_equation_advanced_coherent =
      input->blend_equation_advanced_coherent;
  return result;
}

GpuInfoPtr
TypeConverter<GpuInfoPtr, gpu::GPUInfo>::Convert(
    const gpu::GPUInfo& input) {
  GpuInfoPtr result(GpuInfo::New());
  result->vendor_id = input.gpu.vendor_id;
  result->device_id = input.gpu.device_id;
  result->vendor_info = mojo::String::From<std::string>(input.gl_vendor);
  result->renderer_info = mojo::String::From<std::string>(input.gl_renderer);
  result->driver_version =
      mojo::String::From<std::string>(input.driver_version);
  return result.Pass();
}

}  // namespace mojo

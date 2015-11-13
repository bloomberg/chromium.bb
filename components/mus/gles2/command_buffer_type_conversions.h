// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_
#define COMPONENTS_MUS_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_

#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<mus::mojom::CommandBufferStatePtr,
                     gpu::CommandBuffer::State> {
  static mus::mojom::CommandBufferStatePtr Convert(
      const gpu::CommandBuffer::State& input);
};

template <>
struct TypeConverter<gpu::CommandBuffer::State,
                     mus::mojom::CommandBufferStatePtr> {
  static gpu::CommandBuffer::State Convert(
      const mus::mojom::CommandBufferStatePtr& input);
};

template <>
struct TypeConverter<mus::mojom::GpuShaderPrecisionPtr,
                     gpu::Capabilities::ShaderPrecision> {
  static mus::mojom::GpuShaderPrecisionPtr Convert(
      const gpu::Capabilities::ShaderPrecision& input);
};

template <>
struct TypeConverter<gpu::Capabilities::ShaderPrecision,
                     mus::mojom::GpuShaderPrecisionPtr> {
  static gpu::Capabilities::ShaderPrecision Convert(
      const mus::mojom::GpuShaderPrecisionPtr& input);
};

template <>
struct TypeConverter<mus::mojom::GpuPerStagePrecisionsPtr,
                     gpu::Capabilities::PerStagePrecisions> {
  static mus::mojom::GpuPerStagePrecisionsPtr Convert(
      const gpu::Capabilities::PerStagePrecisions& input);
};

template <>
struct TypeConverter<gpu::Capabilities::PerStagePrecisions,
                     mus::mojom::GpuPerStagePrecisionsPtr> {
  static gpu::Capabilities::PerStagePrecisions Convert(
      const mus::mojom::GpuPerStagePrecisionsPtr& input);
};

template <>
struct TypeConverter<mus::mojom::GpuCapabilitiesPtr, gpu::Capabilities> {
  static mus::mojom::GpuCapabilitiesPtr Convert(const gpu::Capabilities& input);
};

template <>
struct TypeConverter<gpu::Capabilities, mus::mojom::GpuCapabilitiesPtr> {
  static gpu::Capabilities Convert(const mus::mojom::GpuCapabilitiesPtr& input);
};

template <>
struct TypeConverter<mus::mojom::GpuInfoPtr, gpu::GPUInfo> {
  static mus::mojom::GpuInfoPtr Convert(const gpu::GPUInfo& input);
};

}  // namespace mojo

#endif  // COMPONENTS_MUS_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_

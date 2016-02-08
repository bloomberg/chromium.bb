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
struct TypeConverter<mus::mojom::GpuInfoPtr, gpu::GPUInfo> {
  static mus::mojom::GpuInfoPtr Convert(const gpu::GPUInfo& input);
};

}  // namespace mojo

#endif  // COMPONENTS_MUS_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_

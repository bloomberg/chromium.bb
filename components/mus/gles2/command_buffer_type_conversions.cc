// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_type_conversions.h"

#include "components/mus/public/interfaces/command_buffer.mojom.h"

using mus::mojom::GpuInfo;
using mus::mojom::GpuInfoPtr;

namespace mojo {

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
  return result;
}

}  // namespace mojo

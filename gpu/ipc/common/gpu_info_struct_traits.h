// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_GPU_STRUCT_TRAITS_H_
#define CC_IPC_GPU_STRUCT_TRAITS_H_

#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/gpu_info.mojom.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::GpuDevice, gpu::GPUInfo::GPUDevice> {
  static bool Read(gpu::mojom::GpuDeviceDataView data,
                   gpu::GPUInfo::GPUDevice* out);

  static uint32_t vendor_id(const gpu::GPUInfo::GPUDevice& input) {
    return input.vendor_id;
  }

  static uint32_t device_id(const gpu::GPUInfo::GPUDevice& input) {
    return input.device_id;
  }

  static bool active(const gpu::GPUInfo::GPUDevice& input) {
    return input.active;
  }

  static const std::string& vendor_string(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.vendor_string;
  }

  static const std::string& device_string(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.device_string;
  }
};

template <>
struct EnumTraits<gpu::mojom::CollectInfoResult, gpu::CollectInfoResult> {
  static gpu::mojom::CollectInfoResult ToMojom(
      gpu::CollectInfoResult collect_info_result);

  static bool FromMojom(gpu::mojom::CollectInfoResult input,
                        gpu::CollectInfoResult* out);
};

}  // namespace mojo
#endif  // CC_IPC_GPU_STRUCT_TRAITS_H_

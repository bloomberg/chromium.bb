// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_DEVICE_PERF_INFO_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_DEVICE_PERF_INFO_MOJOM_TRAITS_H_

#include "gpu/ipc/common/device_perf_info.mojom-shared.h"

#include "build/build_config.h"
#include "gpu/config/device_perf_info.h"

namespace mojo {

#if defined(OS_WIN)
template <>
struct EnumTraits<gpu::mojom::Direct3DFeatureLevel, D3D_FEATURE_LEVEL> {
  static gpu::mojom::Direct3DFeatureLevel ToMojom(
      D3D_FEATURE_LEVEL d3d_feature_level);
  static bool FromMojom(gpu::mojom::Direct3DFeatureLevel input,
                        D3D_FEATURE_LEVEL* out);
};
#endif  // OS_WIN

template <>
struct EnumTraits<gpu::mojom::HasDiscreteGpu, gpu::HasDiscreteGpu> {
  static gpu::mojom::HasDiscreteGpu ToMojom(
      gpu::HasDiscreteGpu has_discrete_gpu);
  static bool FromMojom(gpu::mojom::HasDiscreteGpu input,
                        gpu::HasDiscreteGpu* out);
};

template <>
struct StructTraits<gpu::mojom::DevicePerfInfoDataView, gpu::DevicePerfInfo> {
  static bool Read(gpu::mojom::DevicePerfInfoDataView data,
                   gpu::DevicePerfInfo* out);

  static uint32_t total_physical_memory_mb(const gpu::DevicePerfInfo& info) {
    return info.total_physical_memory_mb;
  }

  static uint32_t total_disk_space_mb(const gpu::DevicePerfInfo& info) {
    return info.total_disk_space_mb;
  }

  static uint32_t hardware_concurrency(const gpu::DevicePerfInfo& info) {
    return info.hardware_concurrency;
  }

#if defined(OS_WIN)
  static uint32_t system_commit_limit_mb(const gpu::DevicePerfInfo& info) {
    return info.system_commit_limit_mb;
  }

  static D3D_FEATURE_LEVEL d3d11_feature_level(
      const gpu::DevicePerfInfo& info) {
    return info.d3d11_feature_level;
  }

  static gpu::HasDiscreteGpu has_discrete_gpu(const gpu::DevicePerfInfo& info) {
    return info.has_discrete_gpu;
  }
#endif
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_DEVICE_PERF_INFO_MOJOM_TRAITS_H_

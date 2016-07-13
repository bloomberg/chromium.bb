// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_info_struct_traits.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::GpuDevice, gpu::GPUInfo::GPUDevice>::Read(
    gpu::mojom::GpuDeviceDataView data,
    gpu::GPUInfo::GPUDevice* out) {
  out->vendor_id = data.vendor_id();
  out->device_id = data.device_id();
  out->active = data.active();
  return data.ReadVendorString(&out->vendor_string) &&
         data.ReadDeviceString(&out->device_string);
}

// static
gpu::mojom::CollectInfoResult
EnumTraits<gpu::mojom::CollectInfoResult, gpu::CollectInfoResult>::ToMojom(
    gpu::CollectInfoResult collect_info_result) {
  switch (collect_info_result) {
    case gpu::CollectInfoResult::kCollectInfoNone:
      return gpu::mojom::CollectInfoResult::kCollectInfoNone;
    case gpu::CollectInfoResult::kCollectInfoSuccess:
      return gpu::mojom::CollectInfoResult::kCollectInfoSuccess;
    case gpu::CollectInfoResult::kCollectInfoNonFatalFailure:
      return gpu::mojom::CollectInfoResult::kCollectInfoNonFatalFailure;
    case gpu::CollectInfoResult::kCollectInfoFatalFailure:
      return gpu::mojom::CollectInfoResult::kCollectInfoFatalFailure;
  }
  NOTREACHED() << "Invalid CollectInfoResult value:" << collect_info_result;
  return gpu::mojom::CollectInfoResult::kCollectInfoNone;
}

// static
bool EnumTraits<gpu::mojom::CollectInfoResult, gpu::CollectInfoResult>::
    FromMojom(gpu::mojom::CollectInfoResult input,
              gpu::CollectInfoResult* out) {
  switch (input) {
    case gpu::mojom::CollectInfoResult::kCollectInfoNone:
      *out = gpu::CollectInfoResult::kCollectInfoNone;
      return true;
    case gpu::mojom::CollectInfoResult::kCollectInfoSuccess:
      *out = gpu::CollectInfoResult::kCollectInfoSuccess;
      return true;
    case gpu::mojom::CollectInfoResult::kCollectInfoNonFatalFailure:
      *out = gpu::CollectInfoResult::kCollectInfoNonFatalFailure;
      return true;
    case gpu::mojom::CollectInfoResult::kCollectInfoFatalFailure:
      *out = gpu::CollectInfoResult::kCollectInfoFatalFailure;
      return true;
  }
  NOTREACHED() << "Invalid CollectInfoResult value:" << input;
  return false;
}

}  // namespace mojo

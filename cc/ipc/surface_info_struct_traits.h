// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SURFACE_INFO_STRUCT_TRAITS_H_
#define CC_IPC_SURFACE_INFO_STRUCT_TRAITS_H_

#include "cc/ipc/surface_info.mojom-shared.h"
#include "cc/surfaces/surface_info.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::SurfaceInfoDataView, cc::SurfaceInfo> {
  static const cc::SurfaceId& surface_id(const cc::SurfaceInfo& surface_info) {
    return surface_info.id();
  }

  static float device_scale_factor(const cc::SurfaceInfo& surface_info) {
    return surface_info.device_scale_factor();
  }

  static const gfx::Size& size_in_pixels(const cc::SurfaceInfo& surface_info) {
    return surface_info.size_in_pixels();
  }

  static bool Read(cc::mojom::SurfaceInfoDataView data, cc::SurfaceInfo* out) {
    out->device_scale_factor_ = data.device_scale_factor();
    return data.ReadSurfaceId(&out->id_) &&
           data.ReadSizeInPixels(&out->size_in_pixels_) && out->is_valid();
  }
};

}  // namespace mojo

#endif  // CC_IPC_SURFACE_INFO_STRUCT_TRAITS_H_

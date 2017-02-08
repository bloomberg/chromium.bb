// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_VIDEO_ACCELERATOR_STRUCT_TRAITS_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_VIDEO_ACCELERATOR_STRUCT_TRAITS_H_

#include "components/arc/common/video_accelerator.mojom.h"
#include "components/arc/video_accelerator/video_accelerator.h"

namespace mojo {

template <>
struct StructTraits<arc::mojom::ArcVideoAcceleratorDmabufPlaneDataView,
                    arc::ArcVideoAcceleratorDmabufPlane> {
  static uint32_t offset(const arc::ArcVideoAcceleratorDmabufPlane& r) {
    DCHECK_GE(r.offset, 0);
    return r.offset;
  }

  static uint32_t stride(const arc::ArcVideoAcceleratorDmabufPlane& r) {
    DCHECK_GE(r.stride, 0);
    return r.stride;
  }

  static bool Read(arc::mojom::ArcVideoAcceleratorDmabufPlaneDataView data,
                   arc::ArcVideoAcceleratorDmabufPlane* out);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_VIDEO_ACCELERATOR_STRUCT_TRAITS_H_

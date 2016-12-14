// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/video_accelerator_struct_traits.h"

namespace mojo {

// static
bool StructTraits<arc::mojom::ArcVideoAcceleratorDmabufPlaneDataView,
                  arc::ArcVideoAcceleratorDmabufPlane>::
    Read(arc::mojom::ArcVideoAcceleratorDmabufPlaneDataView data,
         arc::ArcVideoAcceleratorDmabufPlane* out) {
  if (data.offset() < 0 || data.stride() < 0) {
    return false;
  }

  out->offset = data.offset();
  out->stride = data.stride();
  return true;
}

}  // namespace mojo

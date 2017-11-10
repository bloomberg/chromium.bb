// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/video_accelerator_struct_traits.h"

namespace mojo {

// static
bool StructTraits<arc::mojom::VideoFramePlaneDataView, arc::VideoFramePlane>::
    Read(arc::mojom::VideoFramePlaneDataView data, arc::VideoFramePlane* out) {
  if (data.offset() < 0 || data.stride() < 0)
    return false;

  out->offset = data.offset();
  out->stride = data.stride();
  return true;
}

// static
bool StructTraits<arc::mojom::SizeDataView, gfx::Size>::Read(
    arc::mojom::SizeDataView data,
    gfx::Size* out) {
  if (data.width() < 0 || data.height() < 0)
    return false;

  out->SetSize(data.width(), data.height());
  return true;
}
}  // namespace mojo

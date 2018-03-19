// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/render_frame_metadata_struct_traits.h"

#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
bool StructTraits<content::mojom::RenderFrameMetadataDataView,
                  cc::RenderFrameMetadata>::
    Read(content::mojom::RenderFrameMetadataDataView data,
         cc::RenderFrameMetadata* out) {
  out->root_background_color = data.root_background_color();
  return data.ReadRootScrollOffset(&out->root_scroll_offset);
}

}  // namespace mojo

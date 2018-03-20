// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RENDER_FRAME_METADATA_STRUCT_TRAITS_H_
#define CONTENT_COMMON_RENDER_FRAME_METADATA_STRUCT_TRAITS_H_

#include "base/optional.h"
#include "cc/trees/render_frame_metadata.h"
#include "content/common/render_frame_metadata.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::RenderFrameMetadataDataView,
                    cc::RenderFrameMetadata> {
  static SkColor root_background_color(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.root_background_color;
  }

  static base::Optional<gfx::Vector2dF> root_scroll_offset(
      const cc::RenderFrameMetadata& metadata) {
    return metadata.root_scroll_offset;
  }

  static bool Read(content::mojom::RenderFrameMetadataDataView data,
                   cc::RenderFrameMetadata* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_RENDER_FRAME_METADATA_STRUCT_TRAITS_H_

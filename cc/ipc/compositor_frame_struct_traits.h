// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_COMPOSITOR_FRAME_STRUCT_TRAITS_H_
#define CC_IPC_COMPOSITOR_FRAME_STRUCT_TRAITS_H_

#include "cc/ipc/compositor_frame.mojom-shared.h"
#include "cc/output/compositor_frame.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::CompositorFrameDataView, cc::CompositorFrame> {
  static const cc::CompositorFrameMetadata& metadata(
      const cc::CompositorFrame& input) {
    return input.metadata;
  }

  static const std::vector<cc::TransferableResource>& resources(
      const cc::CompositorFrame& input) {
    return input.resource_list;
  }

  static const cc::RenderPassList& passes(const cc::CompositorFrame& input) {
    return input.render_pass_list;
  }

  static bool Read(cc::mojom::CompositorFrameDataView data,
                   cc::CompositorFrame* out);
};

}  // namespace mojo

#endif  // CC_IPC_COMPOSITOR_FRAME_STRUCT_TRAITS_H_

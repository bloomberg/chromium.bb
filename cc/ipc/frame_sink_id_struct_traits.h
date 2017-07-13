// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_FRAME_SINK_ID_STRUCT_TRAITS_H_
#define CC_IPC_FRAME_SINK_ID_STRUCT_TRAITS_H_

#include "cc/ipc/frame_sink_id.mojom-shared.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::FrameSinkIdDataView, viz::FrameSinkId> {
  static uint32_t client_id(const viz::FrameSinkId& frame_sink_id) {
    return frame_sink_id.client_id();
  }

  static uint32_t sink_id(const viz::FrameSinkId& frame_sink_id) {
    return frame_sink_id.sink_id();
  }

  static bool Read(cc::mojom::FrameSinkIdDataView data, viz::FrameSinkId* out) {
    *out = viz::FrameSinkId(data.client_id(), data.sink_id());
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_FRAME_SINK_ID_STRUCT_TRAITS_H_

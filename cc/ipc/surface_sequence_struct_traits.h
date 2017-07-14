// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SURFACE_SEQUENCE_STRUCT_TRAITS_H_
#define CC_IPC_SURFACE_SEQUENCE_STRUCT_TRAITS_H_

#include "cc/ipc/surface_sequence.mojom-shared.h"
#include "components/viz/common/surfaces/surface_sequence.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::SurfaceSequenceDataView, viz::SurfaceSequence> {
  static const viz::FrameSinkId& frame_sink_id(const viz::SurfaceSequence& id) {
    return id.frame_sink_id;
  }

  static uint32_t sequence(const viz::SurfaceSequence& id) {
    return id.sequence;
  }

  static bool Read(cc::mojom::SurfaceSequenceDataView data,
                   viz::SurfaceSequence* out) {
    viz::FrameSinkId frame_sink_id;
    if (!data.ReadFrameSinkId(&frame_sink_id))
      return false;
    *out = viz::SurfaceSequence(frame_sink_id, data.sequence());
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_SURFACE_SEQUENCE_STRUCT_TRAITS_H_

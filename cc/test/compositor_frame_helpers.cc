// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/compositor_frame_helpers.h"
#include "cc/output/compositor_frame.h"

namespace cc {
namespace test {

CompositorFrame MakeCompositorFrame() {
  CompositorFrame frame;
  frame.metadata.begin_frame_ack.source_id = BeginFrameArgs::kManualSourceId;
  frame.metadata.begin_frame_ack.sequence_number =
      BeginFrameArgs::kStartingFrameNumber;
  return frame;
}

}  // namespace test
}  // namespace cc

// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_COMPOSITOR_FRAME_ACK_H_
#define CC_COMPOSITOR_FRAME_ACK_H_

#include "cc/cc_export.h"
#include "cc/transferable_resource.h"

namespace cc {

class CC_EXPORT CompositorFrameAck {
 public:
  CompositorFrameAck();
  ~CompositorFrameAck();

  TransferableResourceArray resources;
};

}  // namespace cc

#endif  // CC_COMPOSITOR_FRAME_ACK_H_

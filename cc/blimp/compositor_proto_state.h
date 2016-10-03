// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_COMPOSITOR_PROTO_STATE_H_
#define CC_BLIMP_COMPOSITOR_PROTO_STATE_H_

#include <vector>

#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace cc {
class SwapPromise;

class CC_EXPORT CompositorProtoState {
 public:
  CompositorProtoState();
  ~CompositorProtoState();

  // The SwapPromises associated with this frame update.
  std::vector<SwapPromise> swap_promises;

  // TODO(khushalsagar): Add serialized representation of the layers, layer tree
  // and display lists.

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorProtoState);
};

}  // namespace cc

#endif  // CC_BLIMP_COMPOSITOR_PROTO_STATE_H_

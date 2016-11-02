// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_CLIENT_H_
#define CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_CLIENT_H_

#include <unordered_map>

#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace gfx {
class ScrollOffset;
}  // namespace gfx

namespace cc {
class CompositorProtoState;

class CC_EXPORT RemoteCompositorBridgeClient {
 public:
  using ScrollOffsetMap = std::unordered_map<int, gfx::ScrollOffset>;

  virtual ~RemoteCompositorBridgeClient() {}

  // Called in response to a ScheduleMainFrame request made on the
  // RemoteCompositorBridge.
  // Note: The method should always be invoked asynchronously after the request
  // is made.
  virtual void BeginMainFrame() = 0;

  // Provides an update from the mutations made on the client. Returns true if
  // the update could be successfully applied to the engine state. This can
  // fail, for instance, if the layer present in the update was destroyed on the
  // engine.
  virtual bool ApplyScrollAndScaleUpdateFromClient(
      const ScrollOffsetMap& client_scroll_map,
      float client_page_scale) = 0;
};

}  // namespace cc

#endif  // CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_CLIENT_H_

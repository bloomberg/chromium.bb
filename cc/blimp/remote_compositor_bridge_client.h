// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_CLIENT_H_
#define CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_CLIENT_H_

#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace cc {
class CompositorProtoState;

class CC_EXPORT RemoteCompositorBridgeClient {
 public:
  virtual ~RemoteCompositorBridgeClient() {}

  // Called in response to a ScheduleMainFrame request made on the
  // RemoteCompositorBridge.
  // Note: The method should always be invoked asynchronously after the request
  // is made.
  virtual void BeginMainFrame() = 0;
};

}  // namespace cc

#endif  // CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_CLIENT_H_

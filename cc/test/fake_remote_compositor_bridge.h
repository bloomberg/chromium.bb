// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_REMOTE_COMPOSITOR_BRIDGE_H_
#define CC_TEST_FAKE_REMOTE_COMPOSITOR_BRIDGE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "cc/blimp/compositor_proto_state.h"
#include "cc/blimp/remote_compositor_bridge.h"

namespace cc {

// An implementation of the RemoteCompositorBridge for tests that pump
// BeginMainFrames as soon as the client requests them.
class FakeRemoteCompositorBridge : public RemoteCompositorBridge {
 public:
  FakeRemoteCompositorBridge(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner);
  ~FakeRemoteCompositorBridge() override;

  // RemoteCompositorBridge implementation.
  void BindToClient(RemoteCompositorBridgeClient* client) override;
  void ScheduleMainFrame() override;
  void ProcessCompositorStateUpdate(
      std::unique_ptr<CompositorProtoState> compositor_proto_state) override {}

  bool has_pending_update() const { return has_pending_update_; }

 protected:
  void BeginMainFrame();

  RemoteCompositorBridgeClient* client_;

 private:
  bool has_pending_update_ = false;
  base::WeakPtrFactory<FakeRemoteCompositorBridge> weak_factory_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_REMOTE_COMPOSITOR_BRIDGE_H_

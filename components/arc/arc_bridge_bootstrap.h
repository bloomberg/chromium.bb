// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_BOOTSTRAP_H_
#define COMPONENTS_ARC_ARC_BRIDGE_BOOTSTRAP_H_

#include <memory>

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/arc_bridge.mojom.h"

namespace arc {

// Starts the ARC instance and bootstraps the bridge connection.
// Clients should implement the Delegate to be notified upon communications
// being available.
// The instance can be safely removed 1) before Start() is called, or 2) after
// OnStopped() is called.
// The number of instances must be at most one. Otherwise, ARC instances will
// conflict.
// TODO(hidehiko): This class manages more than "bootstrap" procedure now.
// Rename this to ArcSession.
class ArcBridgeBootstrap {
 public:
  // TODO(hidehiko): Switch to Observer style, which fits more for this design.
  class Delegate {
   public:
    // Called when the connection with ARC instance has been established.
    // TODO(hidehiko): Moving ArcBridgeHost to the ArcBridgeBootstrapImpl
    // so that this can be replaced by OnReady() simply.
    virtual void OnConnectionEstablished(
        mojom::ArcBridgeInstancePtr instance_ptr) = 0;

    // Called when ARC instance is stopped. This is called exactly once
    // per instance which is Start()ed.
    virtual void OnStopped(ArcBridgeService::StopReason reason) = 0;
  };

  // Creates a default instance of ArcBridgeBootstrap.
  static std::unique_ptr<ArcBridgeBootstrap> Create();
  virtual ~ArcBridgeBootstrap() = default;

  // This must be called before calling Start() or Stop(). |delegate| is owned
  // by the caller and must outlive this instance.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Starts and bootstraps a connection with the instance. The Delegate's
  // OnConnectionEstablished() will be called if the bootstrapping is
  // successful, or OnStopped() if it is not.
  // Start() should not be called twice or more.
  virtual void Start() = 0;

  // Requests to stop the currently-running instance.
  // The completion is notified via OnStopped() of the Delegate.
  virtual void Stop() = 0;

 protected:
  ArcBridgeBootstrap() = default;

  // Owned by the caller.
  Delegate* delegate_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcBridgeBootstrap);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_BOOTSTRAP_H_

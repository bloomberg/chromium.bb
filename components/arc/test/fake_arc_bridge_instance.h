// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_INSTANCE_H_

#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/arc_bridge.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class FakeArcBridgeInstance : public mojom::ArcBridgeInstance {
 public:
  FakeArcBridgeInstance();
  ~FakeArcBridgeInstance() override;

  // Finalizes the connection between the host and the instance, and signals
  // the host that the boot sequence has finished.
  void Bind(mojo::InterfaceRequest<mojom::ArcBridgeInstance> interface_request);

  // Resets the binding. Useful to simulate a restart.
  void Unbind();

  // ArcBridgeInstance:
  void Init(mojom::ArcBridgeHostPtr host) override;

  // Ensures the call to Init() has been dispatched.
  void WaitForInitCall();

  // The number of times Init() has been called.
  int init_calls() const { return init_calls_; }

 private:
  // Mojo endpoints.
  mojo::Binding<mojom::ArcBridgeInstance> binding_;
  mojom::ArcBridgeHostPtr host_ptr_;
  int init_calls_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeArcBridgeInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_INSTANCE_H_

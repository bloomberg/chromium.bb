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

class FakeArcBridgeInstance : public ArcBridgeInstance {
 public:
  FakeArcBridgeInstance();
  ~FakeArcBridgeInstance() override;

  // Finalizes the connection between the host and the instance, and signals
  // the host that the boot sequence has finished.
  void Bind(mojo::InterfaceRequest<ArcBridgeInstance> interface_request);

  // ArcBridgeInstance:
  void Init(ArcBridgeHostPtr host) override;

 private:
  // Mojo endpoints.
  mojo::Binding<ArcBridgeInstance> binding_;
  ArcBridgeHostPtr host_ptr_;

  DISALLOW_COPY_AND_ASSIGN(FakeArcBridgeInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_INSTANCE_H_

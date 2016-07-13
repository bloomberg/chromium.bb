// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_BOOTSTRAP_H_
#define COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_BOOTSTRAP_H_

#include "base/macros.h"
#include "components/arc/arc_bridge_bootstrap.h"
#include "components/arc/test/fake_arc_bridge_instance.h"

namespace arc {

// A fake ArcBridgeBootstrap that creates a local connection.
class FakeArcBridgeBootstrap : public ArcBridgeBootstrap,
                               public FakeArcBridgeInstance::Delegate {
 public:
  explicit FakeArcBridgeBootstrap(FakeArcBridgeInstance* instance);
  ~FakeArcBridgeBootstrap() override = default;

  // ArcBridgeBootstrap:
  void Start() override;
  void Stop() override;

 private:
  // FakeArcBridgeInstance::Delegate:
  void OnStopped(ArcBridgeService::StopReason reason) override;

  // Owned by the caller.
  FakeArcBridgeInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(FakeArcBridgeBootstrap);
};

} // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_BOOTSTRAP_H_

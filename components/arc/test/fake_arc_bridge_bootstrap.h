// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_BOOTSTRAP_H_
#define COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_BOOTSTRAP_H_

#include <memory>

#include "base/macros.h"
#include "components/arc/arc_bridge_bootstrap.h"

namespace arc {

// A fake ArcBridgeBootstrap that creates a local connection.
class FakeArcBridgeBootstrap : public ArcBridgeBootstrap {
 public:
  FakeArcBridgeBootstrap();
  ~FakeArcBridgeBootstrap() override;

  // ArcBridgeBootstrap:
  void Start() override;
  void Stop() override;

  // To emulate unexpected stop, such as crash.
  void StopWithReason(ArcBridgeService::StopReason reason);

  // The following control Start() behavior for testing various situations.

  // Enables/disables boot failure emulation, in which OnStopped(reason) will
  // be called when Start() is called.
  void EnableBootFailureEmulation(ArcBridgeService::StopReason reason);

  // Emulate Start() is suspended at some phase, before OnReady() is invoked.
  void SuspendBoot();

  // Returns FakeArcBridgeBootstrap instance. This can be used for a factory
  // in ArcBridgeServiceImpl.
  static std::unique_ptr<ArcBridgeBootstrap> Create();

 private:
  bool boot_failure_emulation_enabled_ = false;
  ArcBridgeService::StopReason boot_failure_reason_;

  bool boot_suspended_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeArcBridgeBootstrap);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_BOOTSTRAP_H_

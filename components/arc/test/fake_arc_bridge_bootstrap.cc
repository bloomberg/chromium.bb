// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_bridge_bootstrap.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace arc {

FakeArcBridgeBootstrap::FakeArcBridgeBootstrap() = default;

FakeArcBridgeBootstrap::~FakeArcBridgeBootstrap() = default;

void FakeArcBridgeBootstrap::Start() {
  if (boot_failure_emulation_enabled_) {
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnStopped(boot_failure_reason_));
  } else if (!boot_suspended_) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnReady());
  }
}

void FakeArcBridgeBootstrap::Stop() {
  StopWithReason(ArcBridgeService::StopReason::SHUTDOWN);
}

void FakeArcBridgeBootstrap::StopWithReason(
    ArcBridgeService::StopReason reason) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnStopped(reason));
}

void FakeArcBridgeBootstrap::EnableBootFailureEmulation(
    ArcBridgeService::StopReason reason) {
  DCHECK(!boot_failure_emulation_enabled_);
  DCHECK(!boot_suspended_);

  boot_failure_emulation_enabled_ = true;
  boot_failure_reason_ = reason;
}

void FakeArcBridgeBootstrap::SuspendBoot() {
  DCHECK(!boot_failure_emulation_enabled_);
  DCHECK(!boot_suspended_);

  boot_suspended_ = true;
}

// static
std::unique_ptr<ArcBridgeBootstrap> FakeArcBridgeBootstrap::Create() {
  return base::MakeUnique<FakeArcBridgeBootstrap>();
}

}  // namespace arc

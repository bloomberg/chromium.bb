// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_bridge_service.h"

namespace arc {

FakeArcBridgeService::FakeArcBridgeService() {
}

FakeArcBridgeService::~FakeArcBridgeService() {
  SetStopped();
}

void FakeArcBridgeService::DetectAvailability() {
}

void FakeArcBridgeService::HandleStartup() {
  SetReady();
}

void FakeArcBridgeService::Shutdown() {
  SetStopped();
}

void FakeArcBridgeService::SetReady() {
  if (state() != State::READY)
    SetState(State::READY);
}

void FakeArcBridgeService::SetStopped() {
  if (state() != State::STOPPED)
    SetState(State::STOPPED);
}

bool FakeArcBridgeService::HasObserver(const Observer* observer) {
  return observer_list().HasObserver(observer);
}

}  // namespace arc

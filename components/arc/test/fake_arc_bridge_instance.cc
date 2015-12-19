// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_bridge_instance.h"

#include <utility>

namespace arc {

FakeArcBridgeInstance::FakeArcBridgeInstance() : binding_(this) {}
FakeArcBridgeInstance::~FakeArcBridgeInstance() {}

void FakeArcBridgeInstance::Init(ArcBridgeHostPtr host) {
  host_ptr_ = std::move(host);
}

void FakeArcBridgeInstance::Bind(
    mojo::InterfaceRequest<ArcBridgeInstance> interface_request) {
  binding_.Bind(std::move(interface_request));
}

}  // namespace arc

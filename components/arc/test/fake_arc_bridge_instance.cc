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
  host_ptr_->OnInstanceBootPhase(INSTANCE_BOOT_PHASE_BRIDGE_READY);
  host_ptr_->OnInstanceBootPhase(INSTANCE_BOOT_PHASE_BOOT_COMPLETED);
}

void FakeArcBridgeInstance::RegisterInputDevice(const mojo::String& name,
                                                const mojo::String& device_type,
                                                mojo::ScopedHandle fd) {}

void FakeArcBridgeInstance::SendNotificationEventToAndroid(
    const mojo::String& key,
    ArcNotificationEvent event) {}

void FakeArcBridgeInstance::RefreshAppList() {}

void FakeArcBridgeInstance::LaunchApp(const mojo::String& package,
                                      const mojo::String& activity) {}

void FakeArcBridgeInstance::RequestAppIcon(const mojo::String& package,
                                           const mojo::String& activity,
                                           ScaleFactor scale_factor) {}

void FakeArcBridgeInstance::RequestProcessList() {}

void FakeArcBridgeInstance::Bind(
    mojo::InterfaceRequest<ArcBridgeInstance> interface_request) {
  binding_.Bind(std::move(interface_request));
}

}  // namespace arc

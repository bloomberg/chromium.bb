// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/scoped_virtual_u2f_device.h"

#include "base/bind.h"

namespace device {
namespace test {

// A FidoDiscovery that always vends a single |VirtualU2fDevice|.
class VirtualU2fDeviceDiscovery : public FidoDiscovery {
 public:
  ~VirtualU2fDeviceDiscovery() override = default;

  U2fTransportProtocol GetTransportProtocol() const override {
    return U2fTransportProtocol::kUsbHumanInterfaceDevice;
  }

  void Start() override {
    auto device = std::make_unique<VirtualU2fDevice>();
    AddDevice(std::move(device));
    NotifyDiscoveryStarted(true);
  }

  void Stop() override { NotifyDiscoveryStopped(true); }
};

ScopedVirtualU2fDevice::ScopedVirtualU2fDevice() = default;
ScopedVirtualU2fDevice::~ScopedVirtualU2fDevice() = default;

std::unique_ptr<FidoDiscovery> ScopedVirtualU2fDevice::CreateFidoDiscovery(
    U2fTransportProtocol transport,
    ::service_manager::Connector* connector) {
  if (transport != U2fTransportProtocol::kUsbHumanInterfaceDevice) {
    return nullptr;
  }
  return std::make_unique<VirtualU2fDeviceDiscovery>();
}

}  // namespace test
}  // namespace device

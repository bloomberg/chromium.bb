// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_SCOPED_VIRTUAL_FIDO_DEVICE_H_
#define DEVICE_FIDO_SCOPED_VIRTUAL_FIDO_DEVICE_H_

#include <memory>

#include "base/macros.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"

namespace device {
namespace test {

// Creating a |ScopedVirtualFidoDevice| causes normal device discovery to be
// hijacked while the object is in scope. Instead a |VirtualFidoDevice| will
// always be discovered. This object pretends to be a HID device.
class ScopedVirtualFidoDevice
    : public ::device::internal::ScopedFidoDiscoveryFactory {
 public:
  ScopedVirtualFidoDevice();
  ~ScopedVirtualFidoDevice() override;

  // Sets the FidoTransportProtocol of the FidoDiscovery to be instantiated by
  // this ScopedVirtualFidoDevice. The default is
  // FidoTransportProtocol::kUsbHumanInterfaceDevice.
  //
  // The FidoTransportProtocol of the device instantiated by the FidoDiscovery
  // must be set separately in mutable_state().
  void SetTransport(FidoTransportProtocol transport);

  void SetSupportedProtocol(ProtocolVersion supported_protocol);
  // SetCtap2Config sets the configuration for |VirtualCtap2Device|s and sets
  // the supported protocol to CTAP2.
  void SetCtap2Config(const VirtualCtap2Device::Config& config);
  VirtualFidoDevice::State* mutable_state();

 protected:
  std::unique_ptr<FidoDiscoveryBase> CreateFidoDiscovery(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector) override;

 private:
  ProtocolVersion supported_protocol_ = ProtocolVersion::kU2f;
  FidoTransportProtocol transport_ =
      FidoTransportProtocol::kUsbHumanInterfaceDevice;
  VirtualCtap2Device::Config ctap2_config_;
  scoped_refptr<VirtualFidoDevice::State> state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedVirtualFidoDevice);
};

}  // namespace test
}  // namespace device

#endif  // DEVICE_FIDO_SCOPED_VIRTUAL_FIDO_DEVICE_H_

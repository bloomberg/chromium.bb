// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_SCOPED_VIRTUAL_U2F_DEVICE_H_
#define DEVICE_FIDO_SCOPED_VIRTUAL_U2F_DEVICE_H_

#include <memory>

#include "base/macros.h"
#include "device/fido/fido_discovery.h"
#include "device/fido/virtual_u2f_device.h"

namespace device {
namespace test {

// Creating a |ScopedVirtualU2fDevice| causes normal device discovery to be
// hijacked while the object is in scope. Instead a |VirtualU2fDevice| will
// always be discovered. This object pretends to be a HID device.
class ScopedVirtualU2fDevice
    : public ::device::internal::ScopedFidoDiscoveryFactory {
 public:
  ScopedVirtualU2fDevice();
  ~ScopedVirtualU2fDevice() override;

  VirtualU2fDevice::State* mutable_state();

 protected:
  std::unique_ptr<FidoDiscovery> CreateFidoDiscovery(
      U2fTransportProtocol transport,
      ::service_manager::Connector* connector) override;

 private:
  scoped_refptr<VirtualU2fDevice::State> state_;
  DISALLOW_COPY_AND_ASSIGN(ScopedVirtualU2fDevice);
};

}  // namespace test
}  // namespace device

#endif  // DEVICE_FIDO_SCOPED_VIRTUAL_U2F_DEVICE_H_

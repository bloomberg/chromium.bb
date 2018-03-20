// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/scoped_virtual_u2f_device.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace device {
namespace test {

// A FidoDiscovery that always vends a single |VirtualU2fDevice|.
class VirtualU2fDeviceDiscovery : public FidoDiscovery {
 public:
  explicit VirtualU2fDeviceDiscovery(
      scoped_refptr<VirtualU2fDevice::State> state)
      : FidoDiscovery(U2fTransportProtocol::kUsbHumanInterfaceDevice),
        state_(std::move(state)) {}
  ~VirtualU2fDeviceDiscovery() override = default;

 protected:
  void StartInternal() override {
    auto device = std::make_unique<VirtualU2fDevice>(state_);
    AddDevice(std::move(device));
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&VirtualU2fDeviceDiscovery::NotifyDiscoveryStarted,
                       base::Unretained(this), true /* success */));
  }

 private:
  scoped_refptr<VirtualU2fDevice::State> state_;
  DISALLOW_COPY_AND_ASSIGN(VirtualU2fDeviceDiscovery);
};

ScopedVirtualU2fDevice::ScopedVirtualU2fDevice()
    : state_(new VirtualU2fDevice::State) {}
ScopedVirtualU2fDevice::~ScopedVirtualU2fDevice() = default;

std::unique_ptr<FidoDiscovery> ScopedVirtualU2fDevice::CreateFidoDiscovery(
    U2fTransportProtocol transport,
    ::service_manager::Connector* connector) {
  if (transport != U2fTransportProtocol::kUsbHumanInterfaceDevice) {
    return nullptr;
  }
  return std::make_unique<VirtualU2fDeviceDiscovery>(state_);
}

}  // namespace test
}  // namespace device

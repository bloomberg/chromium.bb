// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_HID_DISCOVERY_H_
#define DEVICE_U2F_U2F_HID_DISCOVERY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "device/u2f/u2f_discovery.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace service_manager {
class Connector;
}

namespace device {

// TODO(crbug/769631): Now the U2F is talking to HID via mojo, once the U2F
// servicification is unblocked, we'll move U2F back to //service/device/.
// Then it will talk to HID via C++ as part of servicifying U2F.

class U2fHidDiscovery : public U2fDiscovery, device::mojom::HidManagerClient {
 public:
  explicit U2fHidDiscovery(service_manager::Connector* connector);
  ~U2fHidDiscovery() override;

  // U2fDiscovery:
  void Start() override;
  void Stop() override;

 private:
  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device_info) override;

  void OnGetDevices(std::vector<device::mojom::HidDeviceInfoPtr> devices);

  service_manager::Connector* connector_;
  device::mojom::HidManagerPtr hid_manager_;
  mojo::AssociatedBinding<device::mojom::HidManagerClient> binding_;
  HidDeviceFilter filter_;
  base::WeakPtrFactory<U2fHidDiscovery> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(U2fHidDiscovery);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_HID_DISCOVERY_H_

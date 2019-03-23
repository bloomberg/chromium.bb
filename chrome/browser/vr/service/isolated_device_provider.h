// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_ISOLATED_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_VR_SERVICE_ISOLATED_DEVICE_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace vr {

class VRUiHost;

class IsolatedVRDeviceProvider
    : public device::VRDeviceProvider,
      public device::mojom::IsolatedXRRuntimeProviderClient {
 public:
  IsolatedVRDeviceProvider();
  ~IsolatedVRDeviceProvider() override;

  // If the VR API requires initialization that should happen here.
  void Initialize(base::RepeatingCallback<void(device::mojom::XRDeviceId,
                                               device::mojom::VRDisplayInfoPtr,
                                               device::mojom::XRRuntimePtr)>
                      add_device_callback,
                  base::RepeatingCallback<void(device::mojom::XRDeviceId)>
                      remove_device_callback,
                  base::OnceClosure initialization_complete) override;

  // Returns true if initialization is complete.
  bool Initialized() override;

 private:
  // IsolatedXRRuntimeProviderClient
  void OnDeviceAdded(
      device::mojom::XRRuntimePtr device,
      device::mojom::IsolatedXRGamepadProviderFactoryPtr gamepad_factory,
      device::mojom::XRCompositorHostPtr compositor_host,
      device::mojom::XRDeviceId device_id) override;
  void OnDeviceRemoved(device::mojom::XRDeviceId id) override;
  void OnDevicesEnumerated() override;
  void OnServerError();
  void SetupDeviceProvider();

  bool initialized_ = false;
  int retry_count_ = 0;
  device::mojom::IsolatedXRRuntimeProviderPtr device_provider_;

  base::RepeatingCallback<void(device::mojom::XRDeviceId,
                               device::mojom::VRDisplayInfoPtr,
                               device::mojom::XRRuntimePtr)>
      add_device_callback_;
  base::RepeatingCallback<void(device::mojom::XRDeviceId)>
      remove_device_callback_;
  base::OnceClosure initialization_complete_;
  mojo::Binding<device::mojom::IsolatedXRRuntimeProviderClient> binding_;

  using UiHostMap =
      base::flat_map<device::mojom::XRDeviceId, std::unique_ptr<VRUiHost>>;
  UiHostMap ui_host_map_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_ISOLATED_DEVICE_PROVIDER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_
#define CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_

#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace vr {

class VRDisplayHost;

// This class wraps a physical device's interfaces, and registers for events.
// There is one BrowserXRRuntime per physical device runtime.
// It manages browser-side handling of state, like which VRDisplayHost is
// listening for device activation.
class BrowserXRRuntime : public device::mojom::XRRuntimeEventListener {
 public:
  explicit BrowserXRRuntime(device::mojom::XRRuntimePtr runtime,
                            device::mojom::VRDisplayInfoPtr info);
  ~BrowserXRRuntime() override;

  device::mojom::XRRuntime* GetRuntime() { return runtime_.get(); }

  // Methods called by VRDisplayHost to interact with the runtime's device.
  void OnDisplayHostAdded(VRDisplayHost* display);
  void OnDisplayHostRemoved(VRDisplayHost* display);
  void ExitPresent(VRDisplayHost* display);
  void RequestSession(
      VRDisplayHost* display,
      const device::mojom::XRRuntimeSessionOptionsPtr& options,
      device::mojom::VRDisplayHost::RequestSessionCallback callback);
  VRDisplayHost* GetPresentingDisplayHost() { return presenting_display_host_; }
  void UpdateListeningForActivate(VRDisplayHost* display);
  device::mojom::VRDisplayInfoPtr GetVRDisplayInfo() {
    return display_info_.Clone();
  }

 private:
  // device::XRRuntimeEventListener
  void OnDisplayInfoChanged(
      device::mojom::VRDisplayInfoPtr vr_device_info) override;
  void OnExitPresent() override;
  void OnDeviceActivated(device::mojom::VRDisplayEventReason reason,
                         base::OnceCallback<void(bool)> on_handled) override;
  void OnDeviceIdle(device::mojom::VRDisplayEventReason reason) override;

  void OnInitialDevicePropertiesReceived(
      device::mojom::VRDisplayInfoPtr display_info);
  void StopImmersiveSession();
  void OnListeningForActivate(bool is_listening);
  void OnRequestSessionResult(
      base::WeakPtr<VRDisplayHost> display,
      device::mojom::XRRuntimeSessionOptionsPtr options,
      device::mojom::VRDisplayHost::RequestSessionCallback callback,
      device::mojom::XRSessionPtr session,
      device::mojom::XRSessionControllerPtr immersive_session_controller);

  device::mojom::XRRuntimePtr runtime_;
  device::mojom::XRSessionControllerPtr immersive_session_controller_;

  std::set<VRDisplayHost*> displays_;
  device::mojom::VRDisplayInfoPtr display_info_;

  VRDisplayHost* listening_for_activation_display_host_ = nullptr;
  VRDisplayHost* presenting_display_host_ = nullptr;

  mojo::Binding<device::mojom::XRRuntimeEventListener> binding_;

  base::WeakPtrFactory<BrowserXRRuntime> weak_ptr_factory_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_

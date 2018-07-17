// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_VR_DISPLAY_HOST_H_
#define CHROME_BROWSER_VR_SERVICE_VR_DISPLAY_HOST_H_

#include <map>
#include <memory>

#include "base/macros.h"

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace content {
class RenderFrameHost;
}

namespace device {
class VRDisplayImpl;
}  // namespace device

namespace vr {

class BrowserXrDevice;

// The browser-side host for a device::VRDisplayImpl. Controls access to VR
// APIs like poses and presentation.
class VRDisplayHost : public device::mojom::VRDisplayHost {
 public:
  VRDisplayHost(content::RenderFrameHost* render_frame_host,
                device::mojom::VRServiceClient* service_client);
  ~VRDisplayHost() override;

  // device::mojom::VRDisplayHost
  void RequestSession(
      device::mojom::XRSessionOptionsPtr options,
      bool triggered_by_displayactive,
      device::mojom::VRDisplayHost::RequestSessionCallback callback) override;
  void SupportsSession(device::mojom::XRSessionOptionsPtr options,
                       SupportsSessionCallback callback) override;
  void ExitPresent() override;

  void SetListeningForActivate(bool listening);
  void SetInFocusedFrame(bool in_focused_frame);

  // Notifications when devices are added/removed.
  void OnDeviceRemoved(BrowserXrDevice* device);
  void OnDeviceAdded(BrowserXrDevice* device);

  // Notifications/calls from BrowserXrDevice:
  void OnChanged();
  void OnExitPresent();
  void OnBlur();
  void OnFocus();
  void OnActivate(device::mojom::VRDisplayEventReason reason,
                  base::OnceCallback<void(bool)> on_handled);
  void OnDeactivate(device::mojom::VRDisplayEventReason reason);
  bool ListeningForActivate() { return listening_for_activate_; }
  bool InFocusedFrame() { return in_focused_frame_; }

  base::WeakPtr<VRDisplayHost> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void ReportRequestPresent();
  bool IsAnotherHostPresenting();

  bool InternalSupportsSession(device::mojom::XRSessionOptions* options);
  void OnMagicWindowSessionCreated(
      device::mojom::VRDisplayHost::RequestSessionCallback callback,
      device::mojom::VRMagicWindowProviderPtr session,
      device::mojom::XRSessionControllerPtr controller);
  void OnARSessionCreated(
      vr::BrowserXrDevice* device,
      device::mojom::VRDisplayHost::RequestSessionCallback callback,
      device::mojom::XRSessionPtr session);

  // TODO(https://crbug.com/837538): Instead, check before returning this
  // object.
  bool IsSecureContextRequirementSatisfied();

  device::mojom::VRDisplayInfoPtr GetCurrentVRDisplayInfo();

  bool in_focused_frame_ = false;
  bool listening_for_activate_ = false;

  content::RenderFrameHost* render_frame_host_;
  mojo::Binding<device::mojom::VRDisplayHost> binding_;
  device::mojom::VRDisplayClientPtr client_;

  mojo::InterfacePtrSet<device::mojom::XRSessionController>
      magic_window_controllers_;
  int next_key_ = 0;

  // If we start an immersive session, or are listening to immersive activation,
  // notify this device if we are destroyed.
  BrowserXrDevice* immersive_device_ = nullptr;
  BrowserXrDevice* magic_window_device_ = nullptr;
  BrowserXrDevice* ar_device_ = nullptr;

  base::WeakPtrFactory<VRDisplayHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VRDisplayHost);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_VR_DISPLAY_HOST_H_

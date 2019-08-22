// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_
#define CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_

#include <set>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/public/browser/render_frame_host.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace content {
class WebContents;
}

namespace vr {

class VRServiceImpl;

// This interface is implemented by classes that wish to observer the state of
// the XR service for a particular runtime.  In particular, observers may
// currently know when the browser considers a WebContents presenting to an
// immersive headset.  Implementers of this interface will be called on the main
// browser thread.  Currently this is used on Windows to drive overlays.
class BrowserXRRuntimeObserver : public base::CheckedObserver {
 public:
  virtual void SetVRDisplayInfo(
      device::mojom::VRDisplayInfoPtr display_info) = 0;

  // The parameter |contents| is set when a page starts an immersive WebXR
  // session. There can only be at most one active immersive session for the
  // XRRuntime. Set to null when there is no active immersive session.
  virtual void SetWebXRWebContents(content::WebContents* contents) = 0;
};

// This class wraps a physical device's interfaces, and registers for events.
// There is one BrowserXRRuntime per physical device runtime.  It manages
// browser-side handling of state, like which VRServiceImpl is listening for
// device activation.
class BrowserXRRuntime : public device::mojom::XRRuntimeEventListener {
 public:
  using RequestSessionCallback =
      base::OnceCallback<void(device::mojom::XRSessionPtr)>;
  explicit BrowserXRRuntime(device::mojom::XRDeviceId id,
                            device::mojom::XRRuntimePtr runtime,
                            device::mojom::VRDisplayInfoPtr info);
  ~BrowserXRRuntime() override;

  void ExitVrFromPresentingService();
  bool SupportsFeature(device::mojom::XRSessionFeature feature) const;
  bool SupportsAllFeatures(
      const std::vector<device::mojom::XRSessionFeature>& features) const;

  bool SupportsCustomIPD() const;
  bool SupportsNonEmulatedHeight() const;

  device::mojom::XRRuntime* GetRuntime() { return runtime_.get(); }

  // Methods called by VRServiceImpl to interact with the runtime's device.
  void OnServiceAdded(VRServiceImpl* service);
  void OnServiceRemoved(VRServiceImpl* service);
  void ExitPresent(VRServiceImpl* service);
  void RequestSession(VRServiceImpl* service,
                      const device::mojom::XRRuntimeSessionOptionsPtr& options,
                      RequestSessionCallback callback);
  VRServiceImpl* GetPresentingVRService() { return presenting_service_; }
  void UpdateListeningForActivate(VRServiceImpl* service);

  device::mojom::VRDisplayInfoPtr GetVRDisplayInfo() {
    return display_info_.Clone();
  }
  void InitializeAndGetDisplayInfo(
      content::RenderFrameHost* render_frame_host,
      device::mojom::VRService::GetImmersiveVRDisplayInfoCallback callback);

  // Methods called to support metrics/overlays on Windows.
  void AddObserver(BrowserXRRuntimeObserver* observer) {
    observers_.AddObserver(observer);
    observer->SetVRDisplayInfo(display_info_.Clone());
  }
  void RemoveObserver(BrowserXRRuntimeObserver* observer) {
    observers_.RemoveObserver(observer);
  }
  device::mojom::XRDeviceId GetId() const { return id_; }

 private:
  // device::XRRuntimeEventListener
  void OnDisplayInfoChanged(
      device::mojom::VRDisplayInfoPtr vr_device_info) override;
  void OnExitPresent() override;
  void OnDeviceActivated(device::mojom::VRDisplayEventReason reason,
                         base::OnceCallback<void(bool)> on_handled) override;
  void OnDeviceIdle(device::mojom::VRDisplayEventReason reason) override;

  void StopImmersiveSession();
  void OnListeningForActivate(bool is_listening);
  void OnRequestSessionResult(
      base::WeakPtr<VRServiceImpl> service,
      device::mojom::XRRuntimeSessionOptionsPtr options,
      RequestSessionCallback callback,
      device::mojom::XRSessionPtr session,
      device::mojom::XRSessionControllerPtr immersive_session_controller);
  void OnImmersiveSessionError();
  void OnInitialized();

  device::mojom::XRDeviceId id_;
  device::mojom::XRRuntimePtr runtime_;
  device::mojom::XRSessionControllerPtr immersive_session_controller_;

  std::set<VRServiceImpl*> services_;
  device::mojom::VRDisplayInfoPtr display_info_;

  VRServiceImpl* listening_for_activation_service_ = nullptr;
  VRServiceImpl* presenting_service_ = nullptr;

  mojo::AssociatedBinding<device::mojom::XRRuntimeEventListener> binding_;
  std::vector<device::mojom::VRService::GetImmersiveVRDisplayInfoCallback>
      pending_initialization_callbacks_;

  base::ObserverList<BrowserXRRuntimeObserver> observers_;

  base::WeakPtrFactory<BrowserXRRuntime> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_BROWSER_XR_RUNTIME_H_

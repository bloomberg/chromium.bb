// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_BASE_H
#define DEVICE_VR_VR_DEVICE_BASE_H

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"

namespace device {

class VRDisplayImpl;

// Represents one of the platform's VR devices. Owned by the respective
// VRDeviceProvider.
// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT VRDeviceBase : public VRDevice {
 public:
  VRDeviceBase();
  ~VRDeviceBase() override;

  // VRDevice Implementation
  unsigned int GetId() const override;
  void PauseTracking() override;
  void ResumeTracking() override;
  void Blur() override;
  void Focus() override;
  void OnExitPresent() override;
  mojom::VRDisplayInfoPtr GetVRDisplayInfo() final;

  virtual void RequestPresent(
      VRDisplayImpl* display,
      mojom::VRSubmitFrameClientPtr submit_client,
      mojom::VRPresentationProviderRequest request,
      mojom::VRDisplayHost::RequestPresentCallback callback);
  virtual void ExitPresent();
  virtual void GetPose(mojom::VRMagicWindowProvider::GetPoseCallback callback);

  void AddDisplay(VRDisplayImpl* display);
  void RemoveDisplay(VRDisplayImpl* display);
  bool IsAccessAllowed(VRDisplayImpl* display);
  bool CheckPresentingDisplay(VRDisplayImpl* display);
  void OnListeningForActivateChanged(VRDisplayImpl* display);
  void OnFrameFocusChanged(VRDisplayImpl* display);

  VRDisplayImpl* GetPresentingDisplay() { return presenting_display_; }

 protected:
  void SetPresentingDisplay(VRDisplayImpl* display);
  void SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info);
  void OnActivate(mojom::VRDisplayEventReason reason,
                  base::Callback<void(bool)> on_handled);

  virtual void OnListeningForActivate(bool listening);

 private:
  void UpdateListeningForActivate(VRDisplayImpl* display);

  std::set<VRDisplayImpl*> displays_;

  VRDisplayImpl* presenting_display_ = nullptr;
  VRDisplayImpl* listening_for_activate_diplay_ = nullptr;

  // On Android display activate is triggered after the Device ON flow that
  // pauses Chrome, which unfocuses the webvr page, which lets us know that that
  // page is no longer listening to displayActivate. We then have a race between
  // blink-side getting focus back and letting us know the page is listening for
  // displayactivate, and the browser sending displayactivate.
  // We resolve this by remembering which display was last listening for
  // displayactivate most recently, and sending the activation there so long as
  // the WebContents it belongs to is focused and nothing has more recently
  // started listening for displayactivate.
  // This is safe because if the page is /actually/ not listening for activate
  // anymore, the displayactivate signal will just be ignored.
  VRDisplayImpl* last_listening_for_activate_diplay_ = nullptr;

  mojom::VRDisplayInfoPtr display_info_;

  unsigned int id_;

  static unsigned int next_id_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceBase);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_BASE_H

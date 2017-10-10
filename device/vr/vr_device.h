// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_H
#define DEVICE_VR_VR_DEVICE_H

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"

namespace device {

class VRDisplayImpl;

const unsigned int VR_DEVICE_LAST_ID = 0xFFFFFFFF;

// Represents one of the platform's VR devices. Owned by the respective
// VRDeviceProvider.
// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT VRDevice {
 public:
  VRDevice();
  virtual ~VRDevice();

  unsigned int id() const { return id_; }

  virtual mojom::VRDisplayInfoPtr GetVRDisplayInfo() = 0;
  virtual void RequestPresent(
      VRDisplayImpl* display,
      mojom::VRSubmitFrameClientPtr submit_client,
      mojom::VRPresentationProviderRequest request,
      mojom::VRDisplayHost::RequestPresentCallback callback) = 0;
  virtual void ExitPresent() = 0;
  virtual void GetPose(
      mojom::VRMagicWindowProvider::GetPoseCallback callback) = 0;

  void AddDisplay(VRDisplayImpl* display);
  void RemoveDisplay(VRDisplayImpl* display);
  void OnListeningForActivateChanged(VRDisplayImpl* display);
  void OnFrameFocusChanged(VRDisplayImpl* display);

  bool IsAccessAllowed(VRDisplayImpl* display);
  bool CheckPresentingDisplay(VRDisplayImpl* display);
  VRDisplayImpl* GetPresentingDisplay() { return presenting_display_; }

  virtual void PauseTracking() {}
  virtual void ResumeTracking() {}

  void OnActivate(mojom::VRDisplayEventReason reason,
                  const base::Callback<void(bool)>& on_handled);
  void OnChanged();
  void OnExitPresent();
  void OnBlur();
  void OnFocus();

 protected:
  void SetPresentingDisplay(VRDisplayImpl* display);
  virtual void OnListeningForActivateChanged(bool listening) {}

 private:
  void UpdateListeningForActivate(VRDisplayImpl* display);

  std::set<VRDisplayImpl*> displays_;

  VRDisplayImpl* presenting_display_ = nullptr;
  VRDisplayImpl* listening_for_activate_diplay_ = nullptr;

  unsigned int id_;

  static unsigned int next_id_;

  DISALLOW_COPY_AND_ASSIGN(VRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_H

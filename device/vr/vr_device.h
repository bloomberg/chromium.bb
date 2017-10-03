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
      mojom::VRDisplay::RequestPresentCallback callback) = 0;
  virtual void ExitPresent() = 0;
  virtual void GetNextMagicWindowPose(
      VRDisplayImpl* display,
      mojom::VRDisplay::GetNextMagicWindowPoseCallback callback) = 0;

  void AddDisplay(VRDisplayImpl* display);
  void RemoveDisplay(VRDisplayImpl* display);
  virtual void OnDisplayAdded(VRDisplayImpl* display) {}
  virtual void OnDisplayRemoved(VRDisplayImpl* display) {}
  virtual void OnListeningForActivateChanged(VRDisplayImpl* display) {}

  bool IsAccessAllowed(VRDisplayImpl* display);
  bool CheckPresentingDisplay(VRDisplayImpl* display);
  VRDisplayImpl* GetPresentingDisplay() { return presenting_display_; }

  virtual void PauseTracking() {}
  virtual void ResumeTracking() {}

  void OnChanged();
  void OnExitPresent();
  void OnBlur();
  void OnFocus();

 protected:
  void SetPresentingDisplay(VRDisplayImpl* display);

 private:
  std::set<VRDisplayImpl*> displays_;

  VRDisplayImpl* presenting_display_;

  unsigned int id_;

  static unsigned int next_id_;

  DISALLOW_COPY_AND_ASSIGN(VRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_H

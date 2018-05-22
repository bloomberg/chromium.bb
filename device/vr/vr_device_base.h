// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_BASE_H
#define DEVICE_VR_VR_DEVICE_BASE_H

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "ui/display/display.h"

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
  void OnExitPresent() override;
  mojom::VRDisplayInfoPtr GetVRDisplayInfo() final;
  void SetMagicWindowEnabled(bool enabled) final;

  virtual void RequestPresent(
      VRDisplayImpl* display,
      mojom::VRSubmitFrameClientPtr submit_client,
      mojom::VRPresentationProviderRequest request,
      mojom::VRRequestPresentOptionsPtr present_options,
      mojom::VRDisplayHost::RequestPresentCallback callback);
  virtual void ExitPresent();
  bool IsFallbackDevice() override;

  void AddDisplay(VRDisplayImpl* display);
  void RemoveDisplay(VRDisplayImpl* display);
  bool IsAccessAllowed(VRDisplayImpl* display);
  bool CheckPresentingDisplay(VRDisplayImpl* display);
  void OnListeningForActivateChanged(VRDisplayImpl* display);
  void OnFrameFocusChanged(VRDisplayImpl* display);
  void GetMagicWindowPose(
      mojom::VRMagicWindowProvider::GetPoseCallback callback);
  // TODO(https://crbug.com/836478): Rename this, and probably
  // GetMagicWindowPose to GetNonExclusiveFrameData.
  void GetMagicWindowFrameData(
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation,
      mojom::VRMagicWindowProvider::GetFrameDataCallback callback);

  VRDisplayImpl* GetPresentingDisplay() { return presenting_display_; }

 protected:
  void SetPresentingDisplay(VRDisplayImpl* display);
  void SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info);
  void OnActivate(mojom::VRDisplayEventReason reason,
                  base::Callback<void(bool)> on_handled);

 private:
  void UpdateListeningForActivate(VRDisplayImpl* display);
  virtual void OnListeningForActivate(bool listening);
  virtual void OnMagicWindowPoseRequest(
      mojom::VRMagicWindowProvider::GetPoseCallback callback);
  virtual void OnMagicWindowFrameDataRequest(
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation,
      mojom::VRMagicWindowProvider::GetFrameDataCallback callback);

  std::set<VRDisplayImpl*> displays_;

  VRDisplayImpl* presenting_display_ = nullptr;
  VRDisplayImpl* listening_for_activate_diplay_ = nullptr;

  mojom::VRDisplayInfoPtr display_info_;

  unsigned int id_;
  static unsigned int next_id_;
  bool magic_window_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceBase);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_BASE_H

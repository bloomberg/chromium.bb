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
class DEVICE_VR_EXPORT VRDevice {
 public:
  VRDevice();
  virtual ~VRDevice();

  unsigned int id() const { return id_; }

  // Queries VR device for display info and calls onCreated once the display
  // info object is created. If the query fails onCreated will be called with a
  // nullptr as argument. onCreated can be called before this function returns.
  virtual void CreateVRDisplayInfo(
      const base::Callback<void(mojom::VRDisplayInfoPtr)>& on_created) = 0;

  virtual void RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                              mojom::VRPresentationProviderRequest request,
                              const base::Callback<void(bool)>& callback) = 0;
  virtual void SetSecureOrigin(bool secure_origin) = 0;
  virtual void ExitPresent() = 0;
  virtual void GetNextMagicWindowPose(
      VRDisplayImpl* display,
      mojom::VRDisplay::GetNextMagicWindowPoseCallback callback) = 0;

  void AddDisplay(VRDisplayImpl* display);
  void RemoveDisplay(VRDisplayImpl* display);
  virtual void OnDisplayAdded(VRDisplayImpl* display) {}
  virtual void OnDisplayRemoved(VRDisplayImpl* display) {}
  virtual void OnListeningForActivateChanged(VRDisplayImpl* display){};

  bool IsAccessAllowed(VRDisplayImpl* display);
  bool CheckPresentingDisplay(VRDisplayImpl* display);

  void OnChanged();
  void OnExitPresent();
  void OnBlur();
  void OnFocus();

 protected:
  friend class VRDisplayImpl;
  friend class VRDisplayImplTest;

  void SetPresentingDisplay(VRDisplayImpl* display);

 private:
  void OnVRDisplayInfoCreated(mojom::VRDisplayInfoPtr vr_device_info);

  std::set<VRDisplayImpl*> displays_;

  VRDisplayImpl* presenting_display_;

  unsigned int id_;

  static unsigned int next_id_;

  base::WeakPtrFactory<VRDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_H

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_H
#define DEVICE_VR_VR_DEVICE_H

#include "base/macros.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"

namespace blink {
struct WebHMDSensorState;
}

namespace ui {
class BaseWindow;
}

namespace device {

class VRDeviceProvider;

const unsigned int VR_DEVICE_LAST_ID = 0xFFFFFFFF;

class DEVICE_VR_EXPORT VRDevice {
 public:
  explicit VRDevice(VRDeviceProvider* provider);
  virtual ~VRDevice();

  VRDeviceProvider* provider() const { return provider_; }
  unsigned int id() const { return id_; }

  virtual VRDisplayPtr GetVRDevice() = 0;
  virtual VRPosePtr GetPose() = 0;
  virtual void ResetPose() = 0;

  virtual bool RequestPresent(bool secure_origin);
  virtual void ExitPresent(){};
  virtual void SubmitFrame(VRPosePtr pose){};
  virtual void UpdateLayerBounds(VRLayerBoundsPtr leftBounds,
                                 VRLayerBoundsPtr rightBounds){};

 private:
  VRDeviceProvider* provider_;
  unsigned int id_;

  static unsigned int next_id_;

  DISALLOW_COPY_AND_ASSIGN(VRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_H

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

class VRDisplayImpl;
class VRServiceImpl;

const unsigned int VR_DEVICE_LAST_ID = 0xFFFFFFFF;

class DEVICE_VR_EXPORT VRDevice {
 public:
  VRDevice();
  virtual ~VRDevice();

  unsigned int id() const { return id_; }

  virtual mojom::VRDisplayInfoPtr GetVRDevice() = 0;
  virtual mojom::VRPosePtr GetPose(VRServiceImpl* service) = 0;
  virtual void ResetPose(VRServiceImpl* service) = 0;

  virtual bool RequestPresent(VRServiceImpl* service, bool secure_origin) = 0;
  virtual void ExitPresent(VRServiceImpl* service) = 0;
  virtual void SubmitFrame(VRServiceImpl* service, mojom::VRPosePtr pose) = 0;
  virtual void UpdateLayerBounds(VRServiceImpl* service,
                                 mojom::VRLayerBoundsPtr leftBounds,
                                 mojom::VRLayerBoundsPtr rightBounds) = 0;

  virtual void AddService(VRServiceImpl* service);
  virtual void RemoveService(VRServiceImpl* service);

  // TODO(shaobo.yan@intel.com): Checks should be done against VRDisplayImpl and
  // the name should be considered.
  virtual bool IsAccessAllowed(VRServiceImpl* service);
  virtual bool IsPresentingService(VRServiceImpl* service);

  virtual void OnDisplayChanged();
  virtual void OnExitPresent(VRServiceImpl* service);
  virtual void OnDisplayBlur();
  virtual void OnDisplayFocus();

 protected:
  // Each Service have one VRDisplay with one VRDevice.
  // TODO(shaobo.yan@intel.com): Since the VRDisplayImpl knows its VRServiceImpl
  // we should
  // only need to store the VRDisplayImpl.
  using DisplayClientMap = std::map<VRServiceImpl*, VRDisplayImpl*>;
  DisplayClientMap displays_;

  // TODO(shaobo.yan@intel.com): Should track presenting VRDisplayImpl instead.
  VRServiceImpl* presenting_service_;

 private:
  unsigned int id_;

  static unsigned int next_id_;

  DISALLOW_COPY_AND_ASSIGN(VRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_H

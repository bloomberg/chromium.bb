// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VR_VR_DEVICE_H
#define CONTENT_BROWSER_VR_VR_DEVICE_H

#include "base/macros.h"
#include "third_party/WebKit/public/platform/modules/vr/vr_service.mojom.h"

namespace blink {
struct WebHMDSensorState;
}

namespace ui {
class BaseWindow;
}

namespace content {

class VRDeviceProvider;

const unsigned int VR_DEVICE_LAST_ID = 0xFFFFFFFF;

class VRDevice {
 public:
  explicit VRDevice(VRDeviceProvider* provider);
  virtual ~VRDevice();

  VRDeviceProvider* provider() const { return provider_; }
  unsigned int id() const { return id_; }

  virtual blink::mojom::VRDeviceInfoPtr GetVRDevice() = 0;
  virtual blink::mojom::VRSensorStatePtr GetSensorState() = 0;
  virtual void ResetSensor() = 0;

 private:
  VRDeviceProvider* provider_;
  unsigned int id_;

  static unsigned int next_id_;

  DISALLOW_COPY_AND_ASSIGN(VRDevice);
};

}  // namespace content

#endif  // CONTENT_BROWSER_VR_VR_DEVICE_H

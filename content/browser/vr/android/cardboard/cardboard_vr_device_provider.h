// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VR_CARDBOARD_VR_DEVICE_PROVIDER_H
#define CONTENT_BROWSER_VR_CARDBOARD_VR_DEVICE_PROVIDER_H

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/vr/vr_device.h"
#include "content/browser/vr/vr_device_provider.h"

namespace content {

class CardboardVRDeviceProvider : public VRDeviceProvider {
 public:
  CardboardVRDeviceProvider();
  ~CardboardVRDeviceProvider() override;

  void GetDevices(std::vector<VRDevice*>* devices) override;
  void Initialize() override;

 private:
  scoped_ptr<VRDevice> cardboard_device_;

  DISALLOW_COPY_AND_ASSIGN(CardboardVRDeviceProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_VR_CARDBOARD_VR_DEVICE_PROVIDER_H

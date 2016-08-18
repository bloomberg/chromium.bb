// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "device/vr/android/gvr/gvr_api_manager.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"

namespace device {

class GvrDeviceProvider : public VRDeviceProvider, public GvrApiManagerClient {
 public:
  GvrDeviceProvider();
  ~GvrDeviceProvider() override;

  void GetDevices(std::vector<VRDevice*>* devices) override;
  void Initialize() override;

  // GvrApiManagerClient
  void OnGvrApiInitialized(gvr::GvrApi* gvr_api) override;
  void OnGvrApiShutdown() override;

 private:
  std::unique_ptr<VRDevice> vr_device_;
  base::android::ScopedJavaGlobalRef<jobject> j_device_;

  DISALLOW_COPY_AND_ASSIGN(GvrDeviceProvider);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H

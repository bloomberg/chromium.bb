// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VR_CARDBOARD_VR_DEVICE_H
#define CONTENT_BROWSER_VR_CARDBOARD_VR_DEVICE_H

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "content/browser/vr/vr_device.h"

namespace content {

class CardboardVRDevice : public VRDevice {
 public:
  static bool RegisterCardboardVRDevice(JNIEnv* env);

  explicit CardboardVRDevice(VRDeviceProvider* provider);
  ~CardboardVRDevice() override;

  blink::mojom::VRDeviceInfoPtr GetVRDevice() override;
  blink::mojom::VRSensorStatePtr GetSensorState() override;
  void ResetSensor() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_cardboard_device_;
  base::android::ScopedJavaGlobalRef<jfloatArray> j_head_matrix_;

  unsigned int frame_index_;

  DISALLOW_COPY_AND_ASSIGN(CardboardVRDevice);
};

}  // namespace content

#endif  // CONTENT_BROWSER_VR_CARDBOARD_VR_DEVICE_H

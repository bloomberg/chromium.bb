// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_H

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "device/vr/vr_device.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace device {

class GvrDelegateProvider;
class VRDisplayImpl;

// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT GvrDevice : public VRDevice {
 public:
  static std::unique_ptr<GvrDevice> Create();
  ~GvrDevice() override;

  // VRDevice
  mojom::VRDisplayInfoPtr GetVRDisplayInfo() override;
  void RequestPresent(
      VRDisplayImpl* display,
      mojom::VRSubmitFrameClientPtr submit_client,
      mojom::VRPresentationProviderRequest request,
      mojom::VRDisplayHost::RequestPresentCallback callback) override;
  void ExitPresent() override;
  void GetPose(mojom::VRMagicWindowProvider::GetPoseCallback callback) override;
  void OnListeningForActivateChanged(bool listening) override;
  void PauseTracking() override;
  void ResumeTracking() override;

  void OnDIPScaleChanged(JNIEnv* env,
                         const base::android::JavaRef<jobject>& obj);

 private:
  void OnRequestPresentResult(
      mojom::VRDisplayHost::RequestPresentCallback callback,
      VRDisplayImpl* display,
      bool result);
  void UpdateListeningForActivate();

  GvrDevice();
  GvrDelegateProvider* GetGvrDelegateProvider();

  base::android::ScopedJavaGlobalRef<jobject> non_presenting_context_;
  std::unique_ptr<gvr::GvrApi> gvr_api_;
  mojom::VRDisplayInfoPtr display_info_;

  base::WeakPtrFactory<GvrDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GvrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_H

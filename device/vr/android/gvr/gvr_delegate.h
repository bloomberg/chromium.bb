// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DELEGATE_H
#define DEVICE_VR_ANDROID_GVR_DELEGATE_H

#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace gvr {
class GvrApi;
}  // namespace gvr

namespace device {

constexpr gvr::Sizei kInvalidRenderTargetSize = {0, 0};

class DEVICE_VR_EXPORT GvrDelegate {
 public:
  virtual void SetWebVRSecureOrigin(bool secure_origin) = 0;
  virtual void SubmitWebVRFrame() = 0;
  virtual void UpdateWebVRTextureBounds(int16_t frame_index,
                                        const gvr::Rectf& left_bounds,
                                        const gvr::Rectf& right_bounds) = 0;
  virtual gvr::Sizei GetWebVRCompositorSurfaceSize() = 0;
  virtual void SetWebVRRenderSurfaceSize(int width, int height) = 0;
  // TODO(mthiesse): This function is not threadsafe. crbug.com/674594
  virtual gvr::GvrApi* gvr_api() = 0;
  virtual void OnVRVsyncProviderRequest(
      mojom::VRVSyncProviderRequest request) = 0;
  virtual void UpdateVSyncInterval(long timebase_nanos,
                                   double interval_seconds) = 0;

 protected:
  virtual ~GvrDelegate() {}
};

class DEVICE_VR_EXPORT GvrDelegateProvider {
 public:
  static void SetInstance(GvrDelegateProvider* delegate_provider);
  static GvrDelegateProvider* GetInstance();

  virtual void SetDeviceProvider(GvrDeviceProvider* device_provider) = 0;
  virtual void RequestWebVRPresent(
      const base::Callback<void(bool)>& callback) = 0;
  virtual void ExitWebVRPresent() = 0;
  virtual GvrDelegate* GetNonPresentingDelegate() = 0;
  virtual void DestroyNonPresentingDelegate() = 0;
  virtual void SetListeningForActivate(bool listening) = 0;

 protected:
  virtual ~GvrDelegateProvider() {}

 private:
  static GvrDelegateProvider* delegate_provider_;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DELEGATE_H

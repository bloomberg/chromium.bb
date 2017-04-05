// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DELEGATE_H
#define DEVICE_VR_ANDROID_GVR_DELEGATE_H

#include <stdint.h>

#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace device {

class DEVICE_VR_EXPORT GvrDelegate {
 public:
  static mojom::VRPosePtr VRPosePtrFromGvrPose(gvr::Mat4f head_mat);
  static gvr::Sizei GetRecommendedWebVrSize(gvr::GvrApi* gvr_api);
  static mojom::VRDisplayInfoPtr CreateVRDisplayInfo(
      gvr::GvrApi* gvr_api,
      gvr::Sizei recommended_size,
      uint32_t device_id);

  virtual void SetWebVRSecureOrigin(bool secure_origin) = 0;
  virtual void SubmitWebVRFrame(int16_t frame_index,
                                const gpu::MailboxHolder& mailbox) = 0;
  virtual void UpdateWebVRTextureBounds(int16_t frame_index,
                                        const gvr::Rectf& left_bounds,
                                        const gvr::Rectf& right_bounds,
                                        const gvr::Sizei& source_size) = 0;
  virtual void OnVRVsyncProviderRequest(
      mojom::VRVSyncProviderRequest request) = 0;
  virtual void UpdateVSyncInterval(int64_t timebase_nanos,
                                   double interval_seconds) = 0;
  virtual void CreateVRDisplayInfo(
      const base::Callback<void(mojom::VRDisplayInfoPtr)>& callback,
      uint32_t device_id) = 0;

 protected:
  virtual ~GvrDelegate() {}
};

// GvrDelegate, which allows WebVR presentation.
class DEVICE_VR_EXPORT PresentingGvrDelegate : public GvrDelegate {};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DELEGATE_H

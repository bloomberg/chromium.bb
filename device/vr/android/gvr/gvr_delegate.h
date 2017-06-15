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
#include "ui/gfx/geometry/size.h"

namespace gfx {
class RectF;
class Transform;
}  // namespace gfx

namespace gvr {
class GvrApi;
}  // namespace gvr

namespace device {

class DEVICE_VR_EXPORT GvrDelegate {
 public:
  static mojom::VRPosePtr VRPosePtrFromGvrPose(const gfx::Transform& head_mat);
  static void GetGvrPoseWithNeckModel(gvr::GvrApi* gvr_api,
                                      gfx::Transform* out,
                                      int64_t prediction_time);
  static void GetGvrPoseWithNeckModel(gvr::GvrApi* gvr_api,
                                      gfx::Transform* out);
  static mojom::VRPosePtr GetVRPosePtrWithNeckModel(
      gvr::GvrApi* gvr_api,
      gfx::Transform* head_mat_out,
      int64_t prediction_time);
  static mojom::VRPosePtr GetVRPosePtrWithNeckModel(
      gvr::GvrApi* gvr_api,
      gfx::Transform* head_mat_out);
  static gfx::Size GetRecommendedWebVrSize(gvr::GvrApi* gvr_api);
  static mojom::VRDisplayInfoPtr CreateVRDisplayInfo(gvr::GvrApi* gvr_api,
                                                     gfx::Size recommended_size,
                                                     uint32_t device_id);

  virtual void SetWebVRSecureOrigin(bool secure_origin) = 0;
  virtual void SubmitWebVRFrame(int16_t frame_index,
                                const gpu::MailboxHolder& mailbox) = 0;
  virtual void UpdateWebVRTextureBounds(int16_t frame_index,
                                        const gfx::RectF& left_bounds,
                                        const gfx::RectF& right_bounds,
                                        const gfx::Size& source_size) = 0;
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
class DEVICE_VR_EXPORT PresentingGvrDelegate : public GvrDelegate {
 public:
  virtual void SetSubmitClient(
      device::mojom::VRSubmitFrameClientPtr submit_client) = 0;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DELEGATE_H

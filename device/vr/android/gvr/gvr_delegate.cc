// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_delegate.h"

#include "base/trace_event/trace_event.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {
// Default downscale factor for computing the recommended WebVR
// renderWidth/Height from the 1:1 pixel mapped size. Using a rather
// aggressive downscale due to the high overhead of copying pixels
// twice before handing off to GVR. For comparison, the polyfill
// uses approximately 0.55 on a Pixel XL.
static constexpr float kWebVrRecommendedResolutionScale = 0.5;
}  // namespace

/* static */
mojom::VRPosePtr GvrDelegate::VRPosePtrFromGvrPose(gvr::Mat4f head_mat) {
  mojom::VRPosePtr pose = mojom::VRPose::New();

  pose->orientation.emplace(4);

  gfx::Transform inv_transform(
      head_mat.m[0][0], head_mat.m[0][1], head_mat.m[0][2], head_mat.m[0][3],
      head_mat.m[1][0], head_mat.m[1][1], head_mat.m[1][2], head_mat.m[1][3],
      head_mat.m[2][0], head_mat.m[2][1], head_mat.m[2][2], head_mat.m[2][3],
      head_mat.m[3][0], head_mat.m[3][1], head_mat.m[3][2], head_mat.m[3][3]);

  gfx::Transform transform;
  if (inv_transform.GetInverse(&transform)) {
    gfx::DecomposedTransform decomposed_transform;
    gfx::DecomposeTransform(&decomposed_transform, transform);

    pose->orientation.value()[0] = decomposed_transform.quaternion[0];
    pose->orientation.value()[1] = decomposed_transform.quaternion[1];
    pose->orientation.value()[2] = decomposed_transform.quaternion[2];
    pose->orientation.value()[3] = decomposed_transform.quaternion[3];

    pose->position.emplace(3);
    pose->position.value()[0] = decomposed_transform.translate[0];
    pose->position.value()[1] = decomposed_transform.translate[1];
    pose->position.value()[2] = decomposed_transform.translate[2];
  }

  return pose;
}

/* static */
gvr::Sizei GvrDelegate::GetRecommendedWebVrSize(gvr::GvrApi* gvr_api) {
  // Pick a reasonable default size for the WebVR transfer surface
  // based on a downscaled 1:1 render resolution. This size will also
  // be reported to the client via CreateVRDisplayInfo as the
  // client-recommended renderWidth/renderHeight and for the GVR
  // framebuffer. If the client chooses a different size or resizes it
  // while presenting, we'll resize the transfer surface and GVR
  // framebuffer to match.
  gvr::Sizei render_target_size =
      gvr_api->GetMaximumEffectiveRenderTargetSize();
  gvr::Sizei webvr_size = {static_cast<int>(render_target_size.width *
                                            kWebVrRecommendedResolutionScale),
                           static_cast<int>(render_target_size.height *
                                            kWebVrRecommendedResolutionScale)};
  // Ensure that the width is an even number so that the eyes each
  // get the same size, the recommended renderWidth is per eye
  // and the client will use the sum of the left and right width.
  //
  // TODO(klausw,crbug.com/699350): should we round the recommended
  // size to a multiple of 2^N pixels to be friendlier to the GPU? The
  // exact size doesn't matter, and it might be more efficient.
  webvr_size.width &= ~1;

  return webvr_size;
}

/* static */
mojom::VRDisplayInfoPtr GvrDelegate::CreateVRDisplayInfo(
    gvr::GvrApi* gvr_api,
    gvr::Sizei recommended_size,
    uint32_t device_id) {
  TRACE_EVENT0("input", "GvrDelegate::CreateVRDisplayInfo");

  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();

  device->index = device_id;

  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->hasPosition = false;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = true;

  std::string vendor = gvr_api->GetViewerVendor();
  std::string model = gvr_api->GetViewerModel();
  device->displayName = vendor + " " + model;

  gvr::BufferViewportList gvr_buffer_viewports =
      gvr_api->CreateEmptyBufferViewportList();
  gvr_buffer_viewports.SetToRecommendedBufferViewports();

  device->leftEye = mojom::VREyeParameters::New();
  device->rightEye = mojom::VREyeParameters::New();
  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    mojom::VREyeParametersPtr& eye_params =
        (eye == GVR_LEFT_EYE) ? device->leftEye : device->rightEye;
    eye_params->fieldOfView = mojom::VRFieldOfView::New();
    eye_params->offset.resize(3);
    eye_params->renderWidth = recommended_size.width / 2;
    eye_params->renderHeight = recommended_size.height;

    gvr::BufferViewport eye_viewport = gvr_api->CreateBufferViewport();
    gvr_buffer_viewports.GetBufferViewport(eye, &eye_viewport);
    gvr::Rectf eye_fov = eye_viewport.GetSourceFov();
    eye_params->fieldOfView->upDegrees = eye_fov.top;
    eye_params->fieldOfView->downDegrees = eye_fov.bottom;
    eye_params->fieldOfView->leftDegrees = eye_fov.left;
    eye_params->fieldOfView->rightDegrees = eye_fov.right;

    gvr::Mat4f eye_mat = gvr_api->GetEyeFromHeadMatrix(eye);
    eye_params->offset[0] = -eye_mat.m[0][3];
    eye_params->offset[1] = -eye_mat.m[1][3];
    eye_params->offset[2] = -eye_mat.m[2][3];
  }

  return device;
}

}  // namespace device

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_delegate.h"

#include "base/trace_event/trace_event.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/vector3d_f.h"
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

// TODO(mthiesse): If gvr::PlatformInfo().GetPosePredictionTime() is ever
// exposed, use that instead (it defaults to 50ms on most platforms).
static constexpr int64_t kPredictionTimeWithoutVsyncNanos = 50000000;

// Time offset used for calculating angular velocity from a pair of predicted
// poses. The precise value shouldn't matter as long as it's nonzero and much
// less than a frame.
static constexpr int64_t kAngularVelocityEpsilonNanos = 1000000;

void GvrMatToTransform(const gvr::Mat4f& in, gfx::Transform* out) {
  *out = gfx::Transform(in.m[0][0], in.m[0][1], in.m[0][2], in.m[0][3],
                        in.m[1][0], in.m[1][1], in.m[1][2], in.m[1][3],
                        in.m[2][0], in.m[2][1], in.m[2][2], in.m[2][3],
                        in.m[3][0], in.m[3][1], in.m[3][2], in.m[3][3]);
}

gfx::Vector3dF GetAngularVelocityFromPoses(gfx::Transform head_mat,
                                           gfx::Transform head_mat_2,
                                           double epsilon_seconds) {
  // The angular velocity is a 3-element vector pointing along the rotation
  // axis with magnitude equal to rotation speed in radians/second, expressed
  // in the seated frame of reference.
  //
  // The 1.1 spec isn't very clear on details, clarification requested in
  // https://github.com/w3c/webvr/issues/212 . For now, assuming that we
  // want a vector in the sitting reference frame.
  //
  // Assuming that pose prediction is simply based on adding a time * angular
  // velocity rotation to the pose, we can approximate the angular velocity
  // from the difference between two successive poses. This is a first order
  // estimate that assumes small enough rotations so that we can do linear
  // approximation.
  //
  // See:
  // https://en.wikipedia.org/wiki/Angular_velocity#Calculation_from_the_orientation_matrix

  gfx::Transform delta_mat;
  gfx::Transform inverse_head_mat;
  // Calculate difference matrix, and inverse head matrix rotation.
  // For the inverse rotation, just transpose the 3x3 subsection.
  //
  // Assume that epsilon is nonzero since it's based on a compile-time constant
  // provided by the caller.
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 3; ++i) {
      delta_mat.matrix().set(j, i,
                             head_mat_2.matrix().get(j, i) -
                                 head_mat.matrix().get(j, i) / epsilon_seconds);
      inverse_head_mat.matrix().set(j, i, head_mat.matrix().get(i, j));
    }
    delta_mat.matrix().set(j, 3, 0);
    delta_mat.matrix().set(3, j, 0);
    inverse_head_mat.matrix().set(j, 3, 0);
    inverse_head_mat.matrix().set(3, j, 0);
  }
  delta_mat.matrix().set(3, 3, 1);
  inverse_head_mat.matrix().set(3, 3, 1);
  gfx::Transform omega_mat = delta_mat * inverse_head_mat;
  gfx::Vector3dF omega_vec(-omega_mat.matrix().get(2, 1),
                           omega_mat.matrix().get(2, 0),
                           -omega_mat.matrix().get(1, 0));

  // Rotate by inverse head matrix to bring into seated space.
  inverse_head_mat.TransformVector(&omega_vec);
  return omega_vec;
}

}  // namespace

/* static */
mojom::VRPosePtr GvrDelegate::VRPosePtrFromGvrPose(
    const gfx::Transform& head_mat) {
  mojom::VRPosePtr pose = mojom::VRPose::New();

  pose->orientation.emplace(4);

  gfx::Transform inv_transform(head_mat);

  gfx::Transform transform;
  if (inv_transform.GetInverse(&transform)) {
    gfx::DecomposedTransform decomposed_transform;
    gfx::DecomposeTransform(&decomposed_transform, transform);

    pose->orientation.value()[0] = decomposed_transform.quaternion.x();
    pose->orientation.value()[1] = decomposed_transform.quaternion.y();
    pose->orientation.value()[2] = decomposed_transform.quaternion.z();
    pose->orientation.value()[3] = decomposed_transform.quaternion.w();

    pose->position.emplace(3);
    pose->position.value()[0] = decomposed_transform.translate[0];
    pose->position.value()[1] = decomposed_transform.translate[1];
    pose->position.value()[2] = decomposed_transform.translate[2];
  }

  return pose;
}

/* static */
void GvrDelegate::GetGvrPoseWithNeckModel(gvr::GvrApi* gvr_api,
                                          gfx::Transform* out,
                                          int64_t prediction_time) {
  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += prediction_time;

  gvr::Mat4f head_mat = gvr_api->ApplyNeckModel(
      gvr_api->GetHeadSpaceFromStartSpaceRotation(target_time), 1.0f);

  GvrMatToTransform(head_mat, out);
}

/* static */
void GvrDelegate::GetGvrPoseWithNeckModel(gvr::GvrApi* gvr_api,
                                          gfx::Transform* out) {
  GetGvrPoseWithNeckModel(gvr_api, out, kPredictionTimeWithoutVsyncNanos);
}

/* static */
mojom::VRPosePtr GvrDelegate::GetVRPosePtrWithNeckModel(
    gvr::GvrApi* gvr_api,
    gfx::Transform* head_mat_out,
    int64_t prediction_time) {
  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += prediction_time;

  gvr::Mat4f gvr_head_mat = gvr_api->ApplyNeckModel(
      gvr_api->GetHeadSpaceFromStartSpaceRotation(target_time), 1.0f);

  gfx::Transform* head_mat_ptr = head_mat_out;
  gfx::Transform head_mat;
  if (!head_mat_ptr)
    head_mat_ptr = &head_mat;
  GvrMatToTransform(gvr_head_mat, head_mat_ptr);

  mojom::VRPosePtr pose = GvrDelegate::VRPosePtrFromGvrPose(*head_mat_ptr);

  // Get a second pose a bit later to calculate angular velocity.
  target_time.monotonic_system_time_nanos += kAngularVelocityEpsilonNanos;
  gvr::Mat4f gvr_head_mat_2 =
      gvr_api->GetHeadSpaceFromStartSpaceRotation(target_time);
  gfx::Transform head_mat_2;
  GvrMatToTransform(gvr_head_mat_2, &head_mat_2);

  // Add headset angular velocity to the pose.
  pose->angularVelocity.emplace(3);
  double epsilon_seconds = kAngularVelocityEpsilonNanos * 1e-9;
  gfx::Vector3dF angular_velocity =
      GetAngularVelocityFromPoses(*head_mat_ptr, head_mat_2, epsilon_seconds);
  pose->angularVelocity.value()[0] = angular_velocity.x();
  pose->angularVelocity.value()[1] = angular_velocity.y();
  pose->angularVelocity.value()[2] = angular_velocity.z();

  return pose;
}

/* static */
mojom::VRPosePtr GvrDelegate::GetVRPosePtrWithNeckModel(
    gvr::GvrApi* gvr_api,
    gfx::Transform* head_mat_out) {
  return GetVRPosePtrWithNeckModel(gvr_api, head_mat_out,
                                   kPredictionTimeWithoutVsyncNanos);
}

/* static */
gfx::Size GvrDelegate::GetRecommendedWebVrSize(gvr::GvrApi* gvr_api) {
  // Pick a reasonable default size for the WebVR transfer surface
  // based on a downscaled 1:1 render resolution. This size will also
  // be reported to the client via CreateVRDisplayInfo as the
  // client-recommended renderWidth/renderHeight and for the GVR
  // framebuffer. If the client chooses a different size or resizes it
  // while presenting, we'll resize the transfer surface and GVR
  // framebuffer to match.
  gvr::Sizei render_target_size =
      gvr_api->GetMaximumEffectiveRenderTargetSize();

  gfx::Size webvr_size(
      render_target_size.width * kWebVrRecommendedResolutionScale,
      render_target_size.height * kWebVrRecommendedResolutionScale);

  // Ensure that the width is an even number so that the eyes each
  // get the same size, the recommended renderWidth is per eye
  // and the client will use the sum of the left and right width.
  //
  // TODO(klausw,crbug.com/699350): should we round the recommended
  // size to a multiple of 2^N pixels to be friendlier to the GPU? The
  // exact size doesn't matter, and it might be more efficient.
  webvr_size.set_width(webvr_size.width() & ~1);
  return webvr_size;
}

/* static */
mojom::VRDisplayInfoPtr GvrDelegate::CreateVRDisplayInfo(
    gvr::GvrApi* gvr_api,
    gfx::Size recommended_size,
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
    eye_params->renderWidth = recommended_size.width() / 2;
    eye_params->renderHeight = recommended_size.height();

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

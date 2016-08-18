// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device.h"

#include <math.h>
#include <algorithm>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

static const uint64_t kPredictionTimeWithoutVsyncNanos = 50000000;

}  // namespace

GvrDevice::GvrDevice(VRDeviceProvider* provider, gvr::GvrApi* gvr_api)
    : VRDevice(provider), gvr_api_(gvr_api) {}

GvrDevice::~GvrDevice() {}

VRDisplayPtr GvrDevice::GetVRDevice() {
  TRACE_EVENT0("input", "GvrDevice::GetVRDevice");

  VRDisplayPtr device = VRDisplay::New();

  device->capabilities = VRDisplayCapabilities::New();
  device->capabilities->hasOrientation = true;
  device->capabilities->hasPosition = false;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = true;

  device->leftEye = VREyeParameters::New();
  device->rightEye = VREyeParameters::New();
  VREyeParametersPtr& left_eye = device->leftEye;
  VREyeParametersPtr& right_eye = device->rightEye;

  device->displayName = gvr_api_->GetViewerModel();

  gvr::BufferViewportList gvr_buffer_viewports =
      gvr_api_->CreateEmptyBufferViewportList();
  gvr_buffer_viewports.SetToRecommendedBufferViewports();

  gvr::BufferViewport eye_viewport = gvr_api_->CreateBufferViewport();
  gvr_buffer_viewports.GetBufferViewport(GVR_LEFT_EYE, &eye_viewport);
  gvr::Rectf eye_fov = eye_viewport.GetSourceFov();
  left_eye->fieldOfView = VRFieldOfView::New();
  left_eye->fieldOfView->upDegrees = eye_fov.top;
  left_eye->fieldOfView->downDegrees = eye_fov.bottom;
  left_eye->fieldOfView->leftDegrees = eye_fov.left;
  left_eye->fieldOfView->rightDegrees = eye_fov.right;

  eye_viewport = gvr_api_->CreateBufferViewport();
  gvr_buffer_viewports.GetBufferViewport(GVR_RIGHT_EYE, &eye_viewport);
  eye_fov = eye_viewport.GetSourceFov();
  right_eye->fieldOfView = VRFieldOfView::New();
  right_eye->fieldOfView->upDegrees = eye_fov.top;
  right_eye->fieldOfView->downDegrees = eye_fov.bottom;
  right_eye->fieldOfView->leftDegrees = eye_fov.left;
  right_eye->fieldOfView->rightDegrees = eye_fov.right;

  gvr::Mat4f left_eye_mat = gvr_api_->GetEyeFromHeadMatrix(GVR_LEFT_EYE);
  gvr::Mat4f right_eye_mat = gvr_api_->GetEyeFromHeadMatrix(GVR_RIGHT_EYE);

  left_eye->offset = mojo::Array<float>::New(3);
  left_eye->offset[0] = -left_eye_mat.m[0][3];
  left_eye->offset[1] = -left_eye_mat.m[1][3];
  left_eye->offset[2] = -left_eye_mat.m[2][3];

  right_eye->offset = mojo::Array<float>::New(3);
  right_eye->offset[0] = -right_eye_mat.m[0][3];
  right_eye->offset[1] = -right_eye_mat.m[1][3];
  right_eye->offset[2] = -right_eye_mat.m[2][3];

  gvr::Sizei render_target_size = gvr_api_->GetRecommendedRenderTargetSize();

  left_eye->renderWidth = render_target_size.width / 2;
  left_eye->renderHeight = render_target_size.height;

  right_eye->renderWidth = render_target_size.width / 2;
  right_eye->renderHeight = render_target_size.height;

  return device;
}

VRPosePtr GvrDevice::GetPose() {
  TRACE_EVENT0("input", "GvrDevice::GetSensorState");

  VRPosePtr pose = VRPose::New();

  pose->timestamp = base::Time::Now().ToJsTime();

  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += kPredictionTimeWithoutVsyncNanos;

  gvr::Mat4f head_mat = gvr_api_->GetHeadPoseInStartSpace(target_time);

  gfx::Transform inv_transform(
      head_mat.m[0][0], head_mat.m[0][1], head_mat.m[0][2], head_mat.m[0][3],
      head_mat.m[1][0], head_mat.m[1][1], head_mat.m[1][2], head_mat.m[1][3],
      head_mat.m[2][0], head_mat.m[2][1], head_mat.m[2][2], head_mat.m[2][3],
      head_mat.m[3][0], head_mat.m[3][1], head_mat.m[3][2], head_mat.m[3][3]);

  gfx::Transform transform;
  if (inv_transform.GetInverse(&transform)) {
    gfx::DecomposedTransform decomposed_transform;
    gfx::DecomposeTransform(&decomposed_transform, transform);

    pose->orientation = mojo::Array<float>::New(4);
    pose->orientation[0] = decomposed_transform.quaternion[0];
    pose->orientation[1] = decomposed_transform.quaternion[1];
    pose->orientation[2] = decomposed_transform.quaternion[2];
    pose->orientation[3] = decomposed_transform.quaternion[3];

    pose->position = mojo::Array<float>::New(3);
    pose->position[0] = decomposed_transform.translate[0];
    pose->position[1] = decomposed_transform.translate[1];
    pose->position[2] = decomposed_transform.translate[2];
  }

  return pose;
}

void GvrDevice::ResetPose() {
  gvr_api_->ResetTracking();
}

}  // namespace device

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device.h"

#include <math.h>
#include <algorithm>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/vr_device_manager.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

static const uint64_t kPredictionTimeWithoutVsyncNanos = 50000000;

}  // namespace

GvrDevice::GvrDevice(GvrDeviceProvider* provider, GvrDelegate* delegate)
    : VRDevice(provider), delegate_(delegate), gvr_provider_(provider) {}

GvrDevice::~GvrDevice() {}

VRDisplayPtr GvrDevice::GetVRDevice() {
  TRACE_EVENT0("input", "GvrDevice::GetVRDevice");

  VRDisplayPtr device = VRDisplay::New();

  device->index = id();

  device->capabilities = VRDisplayCapabilities::New();
  device->capabilities->hasOrientation = true;
  device->capabilities->hasPosition = false;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = true;

  device->leftEye = VREyeParameters::New();
  device->rightEye = VREyeParameters::New();
  VREyeParametersPtr& left_eye = device->leftEye;
  VREyeParametersPtr& right_eye = device->rightEye;

  left_eye->fieldOfView = VRFieldOfView::New();
  right_eye->fieldOfView = VRFieldOfView::New();

  left_eye->offset = mojo::Array<float>::New(3);
  right_eye->offset = mojo::Array<float>::New(3);

  // TODO(bajones): GVR has a bug that causes it to return bad render target
  // sizes when the phone is in portait mode. Send arbitrary,
  // not-horrifically-wrong values instead.
  //  gvr::Sizei render_target_size = gvr_api->GetRecommendedRenderTargetSize();
  left_eye->renderWidth = 1024;   // render_target_size.width / 2;
  left_eye->renderHeight = 1024;  // render_target_size.height;

  right_eye->renderWidth = left_eye->renderWidth;
  right_eye->renderHeight = left_eye->renderHeight;

  gvr::GvrApi* gvr_api = GetGvrApi();
  if (!gvr_api) {
    // We may not be able to get an instance of GvrApi right away, so
    // stub in some data till we have one.
    device->displayName = "Unknown";

    left_eye->fieldOfView->upDegrees = 45;
    left_eye->fieldOfView->downDegrees = 45;
    left_eye->fieldOfView->leftDegrees = 45;
    left_eye->fieldOfView->rightDegrees = 45;

    right_eye->fieldOfView->upDegrees = 45;
    right_eye->fieldOfView->downDegrees = 45;
    right_eye->fieldOfView->leftDegrees = 45;
    right_eye->fieldOfView->rightDegrees = 45;

    left_eye->offset[0] = -0.0;
    left_eye->offset[1] = -0.0;
    left_eye->offset[2] = -0.03;

    right_eye->offset[0] = 0.0;
    right_eye->offset[1] = 0.0;
    right_eye->offset[2] = 0.03;

    return device;
  }

  std::string vendor = gvr_api->GetViewerVendor();
  std::string model = gvr_api->GetViewerModel();
  device->displayName = vendor + " " + model;

  gvr::BufferViewportList gvr_buffer_viewports =
      gvr_api->CreateEmptyBufferViewportList();
  gvr_buffer_viewports.SetToRecommendedBufferViewports();

  gvr::BufferViewport eye_viewport = gvr_api->CreateBufferViewport();
  gvr_buffer_viewports.GetBufferViewport(GVR_LEFT_EYE, &eye_viewport);
  gvr::Rectf eye_fov = eye_viewport.GetSourceFov();
  left_eye->fieldOfView->upDegrees = eye_fov.top;
  left_eye->fieldOfView->downDegrees = eye_fov.bottom;
  left_eye->fieldOfView->leftDegrees = eye_fov.left;
  left_eye->fieldOfView->rightDegrees = eye_fov.right;

  eye_viewport = gvr_api->CreateBufferViewport();
  gvr_buffer_viewports.GetBufferViewport(GVR_RIGHT_EYE, &eye_viewport);
  eye_fov = eye_viewport.GetSourceFov();
  right_eye->fieldOfView->upDegrees = eye_fov.top;
  right_eye->fieldOfView->downDegrees = eye_fov.bottom;
  right_eye->fieldOfView->leftDegrees = eye_fov.left;
  right_eye->fieldOfView->rightDegrees = eye_fov.right;

  gvr::Mat4f left_eye_mat = gvr_api->GetEyeFromHeadMatrix(GVR_LEFT_EYE);
  left_eye->offset[0] = -left_eye_mat.m[0][3];
  left_eye->offset[1] = -left_eye_mat.m[1][3];
  left_eye->offset[2] = -left_eye_mat.m[2][3];

  gvr::Mat4f right_eye_mat = gvr_api->GetEyeFromHeadMatrix(GVR_RIGHT_EYE);
  right_eye->offset[0] = -right_eye_mat.m[0][3];
  right_eye->offset[1] = -right_eye_mat.m[1][3];
  right_eye->offset[2] = -right_eye_mat.m[2][3];

  return device;
}

VRPosePtr GvrDevice::GetPose() {
  TRACE_EVENT0("input", "GvrDevice::GetSensorState");

  VRPosePtr pose = VRPose::New();

  pose->timestamp = base::Time::Now().ToJsTime();

  // Increment pose frame counter always, even if it's a faked pose.
  pose->poseIndex = ++pose_index_;

  pose->orientation = mojo::Array<float>::New(4);

  gvr::GvrApi* gvr_api = GetGvrApi();
  if (!gvr_api) {
    // If we don't have a GvrApi instance return a static forward orientation.
    pose->orientation[0] = 0.0;
    pose->orientation[1] = 0.0;
    pose->orientation[2] = 0.0;
    pose->orientation[3] = 1.0;

    return pose;
  }

  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += kPredictionTimeWithoutVsyncNanos;

  gvr::Mat4f head_mat =
      gvr_api->GetHeadSpaceFromStartSpaceRotation(target_time);
  head_mat = gvr_api->ApplyNeckModel(head_mat, 1.0f);

  gfx::Transform inv_transform(
      head_mat.m[0][0], head_mat.m[0][1], head_mat.m[0][2], head_mat.m[0][3],
      head_mat.m[1][0], head_mat.m[1][1], head_mat.m[1][2], head_mat.m[1][3],
      head_mat.m[2][0], head_mat.m[2][1], head_mat.m[2][2], head_mat.m[2][3],
      head_mat.m[3][0], head_mat.m[3][1], head_mat.m[3][2], head_mat.m[3][3]);

  gfx::Transform transform;
  if (inv_transform.GetInverse(&transform)) {
    gfx::DecomposedTransform decomposed_transform;
    gfx::DecomposeTransform(&decomposed_transform, transform);

    pose->orientation[0] = decomposed_transform.quaternion[0];
    pose->orientation[1] = decomposed_transform.quaternion[1];
    pose->orientation[2] = decomposed_transform.quaternion[2];
    pose->orientation[3] = decomposed_transform.quaternion[3];

    pose->position = mojo::Array<float>::New(3);
    pose->position[0] = decomposed_transform.translate[0];
    pose->position[1] = decomposed_transform.translate[1];
    pose->position[2] = decomposed_transform.translate[2];
  }

  // Save the underlying GVR pose for use by rendering. It can't use a
  // VRPosePtr since that's a different data type.
  delegate_->SetGvrPoseForWebVr(head_mat, pose_index_);

  return pose;
}

void GvrDevice::ResetPose() {
  gvr::GvrApi* gvr_api = GetGvrApi();
  if (gvr_api)
    gvr_api->ResetTracking();
}

bool GvrDevice::RequestPresent(bool secure_origin) {
  secure_origin_ = secure_origin;
  if (delegate_)
    delegate_->SetWebVRSecureOrigin(secure_origin_);
  return gvr_provider_->RequestPresent();
}

void GvrDevice::ExitPresent() {
  gvr_provider_->ExitPresent();
}

void GvrDevice::SubmitFrame(VRPosePtr pose) {
  if (delegate_)
    delegate_->SubmitWebVRFrame();
}

void GvrDevice::UpdateLayerBounds(VRLayerBoundsPtr leftBounds,
                                  VRLayerBoundsPtr rightBounds) {
  if (!delegate_)
    return;

  delegate_->UpdateWebVRTextureBounds(0,  // Left eye
                                      leftBounds->left, leftBounds->top,
                                      leftBounds->width, leftBounds->height);
  delegate_->UpdateWebVRTextureBounds(1,  // Right eye
                                      rightBounds->left, rightBounds->top,
                                      rightBounds->width, rightBounds->height);
}

void GvrDevice::SetDelegate(GvrDelegate* delegate) {
  delegate_ = delegate;

  // Notify the clients that this device has changed
  if (delegate_) {
    delegate_->SetWebVRSecureOrigin(secure_origin_);
    VRDeviceManager::GetInstance()->OnDeviceChanged(GetVRDevice());
  }
}

gvr::GvrApi* GvrDevice::GetGvrApi() {
  if (!delegate_)
    return nullptr;

  return delegate_->gvr_api();
}

}  // namespace device

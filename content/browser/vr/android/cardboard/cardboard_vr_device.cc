// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vr/android/cardboard/cardboard_vr_device.h"

#include <math.h>
#include <algorithm>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "jni/CardboardVRDevice_jni.h"

using base::android::AttachCurrentThread;

namespace content {

namespace {

// Source:
// http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
VRVector4Ptr MatrixToOrientationQuat(const float m[16]) {
  VRVector4Ptr out = VRVector4::New();
  float trace = m[0] + m[5] + m[10];
  float root;
  if (trace > 0.0f) {
    root = sqrtf(1.0f + trace) * 2.0f;
    out->x = (m[9] - m[6]) / root;
    out->y = (m[2] - m[8]) / root;
    out->z = (m[4] - m[1]) / root;
    out->w = 0.25f * root;
  } else if ((m[0] > m[5]) && (m[0] > m[10])) {
    root = sqrtf(1.0f + m[0] - m[5] - m[10]) * 2.0f;
    out->x = 0.25f * root;
    out->y = (m[1] + m[4]) / root;
    out->z = (m[2] + m[8]) / root;
    out->w = (m[9] - m[6]) / root;
  } else if (m[5] > m[10]) {
    root = sqrtf(1.0f + m[5] - m[0] - m[10]) * 2.0f;
    out->x = (m[1] + m[4]) / root;
    out->y = 0.25f * root;
    out->z = (m[6] + m[9]) / root;
    out->w = (m[2] - m[8]) / root;
  } else {
    root = sqrtf(1.0f + m[10] - m[0] - m[5]) * 2.0f;
    out->x = (m[2] + m[8]) / root;
    out->y = (m[6] + m[9]) / root;
    out->z = 0.25f * root;
    out->w = (m[4] - m[1]) / root;
  }

  return out.Pass();
}

}  // namespace

bool CardboardVRDevice::RegisterCardboardVRDevice(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

CardboardVRDevice::CardboardVRDevice(VRDeviceProvider* provider)
    : VRDevice(provider), frame_index_(0) {
  j_cardboard_device_.Reset(Java_CardboardVRDevice_create(
      AttachCurrentThread(), base::android::GetApplicationContext()));
}

CardboardVRDevice::~CardboardVRDevice() {
  Java_CardboardVRDevice_stopTracking(AttachCurrentThread(),
                                      j_cardboard_device_.obj());
}

VRDeviceInfoPtr CardboardVRDevice::GetVRDevice() {
  VRDeviceInfoPtr device = VRDeviceInfo::New();

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_device_name =
      Java_CardboardVRDevice_getDeviceName(env, j_cardboard_device_.obj());
  device->deviceName =
      base::android::ConvertJavaStringToUTF8(env, j_device_name.obj());

  ScopedJavaLocalRef<jfloatArray> j_fov(env, env->NewFloatArray(4));
  Java_CardboardVRDevice_getFieldOfView(env, j_cardboard_device_.obj(),
                                        j_fov.obj());

  std::vector<float> fov;
  base::android::JavaFloatArrayToFloatVector(env, j_fov.obj(), &fov);

  device->hmdInfo = VRHMDInfo::New();
  VRHMDInfoPtr& hmdInfo = device->hmdInfo;

  hmdInfo->leftEye = VREyeParameters::New();
  hmdInfo->rightEye = VREyeParameters::New();
  VREyeParametersPtr& leftEye = hmdInfo->leftEye;
  VREyeParametersPtr& rightEye = hmdInfo->rightEye;

  leftEye->recommendedFieldOfView = VRFieldOfView::New();
  leftEye->recommendedFieldOfView->upDegrees = fov[0];
  leftEye->recommendedFieldOfView->downDegrees = fov[1];
  leftEye->recommendedFieldOfView->leftDegrees = fov[2];
  leftEye->recommendedFieldOfView->rightDegrees = fov[3];

  // Cardboard devices always assume a mirrored FOV, so this is just the left
  // eye FOV with the left and right degrees swapped.
  rightEye->recommendedFieldOfView = VRFieldOfView::New();
  rightEye->recommendedFieldOfView->upDegrees = fov[0];
  rightEye->recommendedFieldOfView->downDegrees = fov[1];
  rightEye->recommendedFieldOfView->leftDegrees = fov[3];
  rightEye->recommendedFieldOfView->rightDegrees = fov[2];

  // Cardboard does not support configurable FOV.
  leftEye->maximumFieldOfView = leftEye->recommendedFieldOfView.Clone();
  rightEye->maximumFieldOfView = rightEye->recommendedFieldOfView.Clone();
  leftEye->minimumFieldOfView = leftEye->recommendedFieldOfView.Clone();
  rightEye->minimumFieldOfView = rightEye->recommendedFieldOfView.Clone();

  float ipd = Java_CardboardVRDevice_getIpd(env, j_cardboard_device_.obj());

  leftEye->eyeTranslation = VRVector3::New();
  leftEye->eyeTranslation->x = ipd * -0.5f;
  leftEye->eyeTranslation->y = 0.0f;
  leftEye->eyeTranslation->z = 0.0f;

  rightEye->eyeTranslation = VRVector3::New();
  rightEye->eyeTranslation->x = ipd * 0.5f;
  rightEye->eyeTranslation->y = 0.0f;
  rightEye->eyeTranslation->z = 0.0f;

  ScopedJavaLocalRef<jintArray> j_screen_size(env, env->NewIntArray(2));
  Java_CardboardVRDevice_getScreenSize(env, j_cardboard_device_.obj(),
                                       j_screen_size.obj());

  std::vector<int> screen_size;
  base::android::JavaIntArrayToIntVector(env, j_screen_size.obj(),
                                         &screen_size);

  leftEye->renderRect = VRRect::New();
  leftEye->renderRect->x = 0;
  leftEye->renderRect->y = 0;
  leftEye->renderRect->width = screen_size[0] / 2.0;
  leftEye->renderRect->height = screen_size[1];

  rightEye->renderRect = VRRect::New();
  rightEye->renderRect->x = screen_size[0] / 2.0;
  rightEye->renderRect->y = 0;
  rightEye->renderRect->width = screen_size[0] / 2.0;
  rightEye->renderRect->height = screen_size[1];

  return device.Pass();
}

VRSensorStatePtr CardboardVRDevice::GetSensorState() {
  VRSensorStatePtr state = VRSensorState::New();

  state->timestamp = base::Time::Now().ToJsTime();
  state->frameIndex = frame_index_;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jfloatArray> j_head_matrix(env, env->NewFloatArray(16));
  Java_CardboardVRDevice_getSensorState(env, j_cardboard_device_.obj(),
                                        j_head_matrix.obj());

  std::vector<float> head_matrix;
  base::android::JavaFloatArrayToFloatVector(env, j_head_matrix.obj(),
                                             &head_matrix);

  state->orientation = MatrixToOrientationQuat(&head_matrix[0]);

  state->position = VRVector3::New();
  state->position->x = -head_matrix[12];
  state->position->y = head_matrix[13];
  state->position->z = head_matrix[14];

  frame_index_++;

  return state.Pass();
}

void CardboardVRDevice::ResetSensor() {
  Java_CardboardVRDevice_resetSensor(AttachCurrentThread(),
                                     j_cardboard_device_.obj());
}

}  // namespace content

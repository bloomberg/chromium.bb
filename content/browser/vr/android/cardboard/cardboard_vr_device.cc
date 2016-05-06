// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vr/android/cardboard/cardboard_vr_device.h"

#include <math.h>
#include <algorithm>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "jni/CardboardVRDevice_jni.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

using base::android::AttachCurrentThread;

namespace content {

bool CardboardVRDevice::RegisterCardboardVRDevice(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

CardboardVRDevice::CardboardVRDevice(VRDeviceProvider* provider)
    : VRDevice(provider), frame_index_(0) {
  JNIEnv* env = AttachCurrentThread();
  j_cardboard_device_.Reset(Java_CardboardVRDevice_create(
      env, base::android::GetApplicationContext()));
  j_head_matrix_.Reset(env, env->NewFloatArray(16));
}

CardboardVRDevice::~CardboardVRDevice() {
  Java_CardboardVRDevice_stopTracking(AttachCurrentThread(),
                                      j_cardboard_device_.obj());
}

blink::mojom::VRDeviceInfoPtr CardboardVRDevice::GetVRDevice() {
  TRACE_EVENT0("input", "CardboardVRDevice::GetVRDevice");
  blink::mojom::VRDeviceInfoPtr device = blink::mojom::VRDeviceInfo::New();

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

  device->hmdInfo = blink::mojom::VRHMDInfo::New();
  blink::mojom::VRHMDInfoPtr& hmdInfo = device->hmdInfo;

  hmdInfo->leftEye = blink::mojom::VREyeParameters::New();
  hmdInfo->rightEye = blink::mojom::VREyeParameters::New();
  blink::mojom::VREyeParametersPtr& left_eye = hmdInfo->leftEye;
  blink::mojom::VREyeParametersPtr& right_eye = hmdInfo->rightEye;

  left_eye->recommendedFieldOfView = blink::mojom::VRFieldOfView::New();
  left_eye->recommendedFieldOfView->upDegrees = fov[0];
  left_eye->recommendedFieldOfView->downDegrees = fov[1];
  left_eye->recommendedFieldOfView->leftDegrees = fov[2];
  left_eye->recommendedFieldOfView->rightDegrees = fov[3];

  // Cardboard devices always assume a mirrored FOV, so this is just the left
  // eye FOV with the left and right degrees swapped.
  right_eye->recommendedFieldOfView = blink::mojom::VRFieldOfView::New();
  right_eye->recommendedFieldOfView->upDegrees = fov[0];
  right_eye->recommendedFieldOfView->downDegrees = fov[1];
  right_eye->recommendedFieldOfView->leftDegrees = fov[3];
  right_eye->recommendedFieldOfView->rightDegrees = fov[2];

  // Cardboard does not support configurable FOV.
  left_eye->maximumFieldOfView = left_eye->recommendedFieldOfView.Clone();
  right_eye->maximumFieldOfView = right_eye->recommendedFieldOfView.Clone();
  left_eye->minimumFieldOfView = left_eye->recommendedFieldOfView.Clone();
  right_eye->minimumFieldOfView = right_eye->recommendedFieldOfView.Clone();

  float ipd = Java_CardboardVRDevice_getIpd(env, j_cardboard_device_.obj());

  left_eye->eyeTranslation = blink::mojom::VRVector3::New();
  left_eye->eyeTranslation->x = ipd * -0.5f;
  left_eye->eyeTranslation->y = 0.0f;
  left_eye->eyeTranslation->z = 0.0f;

  right_eye->eyeTranslation = blink::mojom::VRVector3::New();
  right_eye->eyeTranslation->x = ipd * 0.5f;
  right_eye->eyeTranslation->y = 0.0f;
  right_eye->eyeTranslation->z = 0.0f;

  ScopedJavaLocalRef<jintArray> j_screen_size(env, env->NewIntArray(2));
  Java_CardboardVRDevice_getScreenSize(env, j_cardboard_device_.obj(),
                                       j_screen_size.obj());

  std::vector<int> screen_size;
  base::android::JavaIntArrayToIntVector(env, j_screen_size.obj(),
                                         &screen_size);

  left_eye->renderRect = blink::mojom::VRRect::New();
  left_eye->renderRect->x = 0;
  left_eye->renderRect->y = 0;
  left_eye->renderRect->width = screen_size[0] / 2.0;
  left_eye->renderRect->height = screen_size[1];

  right_eye->renderRect = blink::mojom::VRRect::New();
  right_eye->renderRect->x = screen_size[0] / 2.0;
  right_eye->renderRect->y = 0;
  right_eye->renderRect->width = screen_size[0] / 2.0;
  right_eye->renderRect->height = screen_size[1];

  return device;
}

blink::mojom::VRSensorStatePtr CardboardVRDevice::GetSensorState() {
  TRACE_EVENT0("input", "CardboardVRDevice::GetSensorState");
  blink::mojom::VRSensorStatePtr state = blink::mojom::VRSensorState::New();

  state->timestamp = base::Time::Now().ToJsTime();
  state->frameIndex = frame_index_++;

  JNIEnv* env = AttachCurrentThread();
  Java_CardboardVRDevice_getSensorState(env, j_cardboard_device_.obj(),
                                        j_head_matrix_.obj());

  std::vector<float> head_matrix;
  base::android::JavaFloatArrayToFloatVector(env, j_head_matrix_.obj(),
                                             &head_matrix);

  gfx::Transform transform(
      head_matrix[0], head_matrix[1], head_matrix[2], head_matrix[3],
      head_matrix[4], head_matrix[5], head_matrix[6], head_matrix[7],
      head_matrix[8], head_matrix[9], head_matrix[10], head_matrix[11],
      head_matrix[12], head_matrix[13], head_matrix[14], head_matrix[15]);

  gfx::DecomposedTransform decomposed_transform;
  gfx::DecomposeTransform(&decomposed_transform, transform);

  state->orientation = blink::mojom::VRVector4::New();
  state->orientation->x = decomposed_transform.quaternion[0];
  state->orientation->y = decomposed_transform.quaternion[1];
  state->orientation->z = decomposed_transform.quaternion[2];
  state->orientation->w = decomposed_transform.quaternion[3];

  state->position = blink::mojom::VRVector3::New();
  state->position->x = decomposed_transform.translate[0];
  state->position->y = decomposed_transform.translate[1];
  state->position->z = decomposed_transform.translate[2];

  return state;
}

void CardboardVRDevice::ResetSensor() {
  Java_CardboardVRDevice_resetSensor(AttachCurrentThread(),
                                     j_cardboard_device_.obj());
}

}  // namespace content

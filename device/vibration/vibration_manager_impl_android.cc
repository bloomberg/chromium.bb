// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vibration/vibration_manager_impl_android.h"

#include "base/bind.h"
#include "device/vibration/vibration_manager_impl.h"
#include "jni/VibrationProvider_jni.h"

using base::android::AttachCurrentThread;

namespace device {

namespace {
const int64 kMinimumVibrationDurationMs = 1;  // 1 millisecond
const int64 kMaximumVibrationDurationMs = 10000;  // 10 seconds
}

// static
VibrationManagerImplAndroid* VibrationManagerImplAndroid::Create() {
  return new VibrationManagerImplAndroid();
}

VibrationManagerImplAndroid::VibrationManagerImplAndroid() {
  j_vibration_provider_.Reset(
      Java_VibrationProvider_create(AttachCurrentThread(),
                                    base::android::GetApplicationContext()));
}

VibrationManagerImplAndroid::~VibrationManagerImplAndroid() {
}

// static
bool VibrationManagerImplAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void VibrationManagerImplAndroid::Vibrate(int64 milliseconds) {
  // Though the Blink implementation already sanitizes vibration times, don't
  // trust any values passed from the client.
  int64 sanitized_milliseconds = std::max(kMinimumVibrationDurationMs,
      std::min(milliseconds, kMaximumVibrationDurationMs));

  Java_VibrationProvider_vibrate(AttachCurrentThread(),
                                 j_vibration_provider_.obj(),
                                 sanitized_milliseconds);
}

void VibrationManagerImplAndroid::Cancel() {
  Java_VibrationProvider_cancelVibration(AttachCurrentThread(),
                                         j_vibration_provider_.obj());
}

// static
void VibrationManagerImpl::Create(
    mojo::InterfaceRequest<VibrationManager> request) {
  BindToRequest(VibrationManagerImplAndroid::Create(), &request);
}

}  // namespace device

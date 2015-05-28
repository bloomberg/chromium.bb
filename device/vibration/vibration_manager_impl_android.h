// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VIBRATION_VIBRATION_MANAGER_IMPL_ANDROID_H_
#define DEVICE_VIBRATION_VIBRATION_MANAGER_IMPL_ANDROID_H_

#include "base/android/jni_android.h"
#include "device/vibration/vibration_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace device {

// TODO(timvolodine): consider implementing the VibrationManager mojo service
// directly in java, crbug.com/439434.
class VibrationManagerImplAndroid : public VibrationManager {
 public:
  static bool Register(JNIEnv* env);

  void Vibrate(int64 milliseconds) override;
  void Cancel() override;

 private:
  friend class VibrationManagerImpl;

  explicit VibrationManagerImplAndroid(
      mojo::InterfaceRequest<VibrationManager> request);
  ~VibrationManagerImplAndroid() override;

  // The binding between this object and the other end of the pipe.
  mojo::StrongBinding<VibrationManager> binding_;

  base::android::ScopedJavaGlobalRef<jobject> j_vibration_provider_;
};

}  // namespace device

#endif  // DEVICE_VIBRATION_VIBRATION_MANAGER_IMPL_ANDROID_H_

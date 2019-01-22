// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_JAVA_UTILS_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_JAVA_UTILS_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/vr/arcore_device/arcore_install_utils.h"

namespace device {
class ArCoreDevice;
}

namespace vr {

class ArCoreJavaUtils : public ArCoreInstallUtils {
 public:
  explicit ArCoreJavaUtils(device::ArCoreDevice* arcore_device);
  ~ArCoreJavaUtils() override;
  bool ShouldRequestInstallArModule() override;
  bool CanRequestInstallArModule() override;
  void RequestInstallArModule(int render_process_id,
                              int render_frame_id) override;
  bool ShouldRequestInstallSupportedArCore() override;
  void RequestInstallSupportedArCore(int render_process_id,
                                     int render_frame_id) override;

  // Methods called from the Java side.
  void OnRequestInstallArModuleResult(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      bool success);
  void OnRequestInstallSupportedArCoreResult(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      bool success);

  bool EnsureLoaded() override;
  base::android::ScopedJavaLocalRef<jobject> GetApplicationContext() override;

 private:
  base::android::ScopedJavaLocalRef<jobject> getTabFromRenderer(
      int render_process_id,
      int render_frame_id);

  device::ArCoreDevice* arcore_device_;
  base::android::ScopedJavaGlobalRef<jobject> j_arcore_java_utils_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_JAVA_UTILS_H_

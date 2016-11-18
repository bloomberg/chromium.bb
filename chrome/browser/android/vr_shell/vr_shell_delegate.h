// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_DELEGATE_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "device/vr/android/gvr/gvr_delegate.h"

namespace device {
class GvrDeviceProvider;
}

namespace vr_shell {

class VrShell;

class VrShellDelegate : public device::GvrDelegateProvider {
 public:
  VrShellDelegate(JNIEnv* env, jobject obj);
  virtual ~VrShellDelegate();

  static VrShellDelegate* GetNativeDelegate(JNIEnv* env, jobject jdelegate);

  base::WeakPtr<device::GvrDeviceProvider> GetDeviceProvider();

  void SetPresentResult(JNIEnv* env, jobject obj, jboolean result);

  // device::GvrDelegateProvider implementation
  void RequestWebVRPresent(
      base::WeakPtr<device::GvrDeviceProvider> device_provider,
      const base::Callback<void(bool)>& callback) override;
  void ExitWebVRPresent() override;
  base::WeakPtr<device::GvrDelegate> GetNonPresentingDelegate() override;
  void DestroyNonPresentingDelegate() override;
  void SetListeningForActivate(bool listening) override;

 private:
  std::unique_ptr<device::GvrDelegate> non_presenting_delegate_;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_delegate_;
  base::WeakPtr<device::GvrDeviceProvider> device_provider_;
  base::Callback<void(bool)> present_callback_;

  DISALLOW_COPY_AND_ASSIGN(VrShellDelegate);
};

bool RegisterVrShellDelegate(JNIEnv* env);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_DELEGATE_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "jni/VrShellDelegate_jni.h"

using base::android::JavaParamRef;
using base::android::AttachCurrentThread;

namespace vr_shell {

VrShellDelegate::VrShellDelegate(JNIEnv* env, jobject obj)
    : device_provider_(nullptr) {
  j_vr_shell_delegate_.Reset(env, obj);
  GvrDelegateProvider::SetInstance(this);
}

VrShellDelegate::~VrShellDelegate() {
  GvrDelegateProvider::SetInstance(nullptr);
}

VrShellDelegate* VrShellDelegate::getNativeDelegate(
    JNIEnv* env, jobject jdelegate) {
  long native_delegate = Java_VrShellDelegate_getNativePointer(env, jdelegate);
  return reinterpret_cast<VrShellDelegate*>(native_delegate);
}

bool VrShellDelegate::ExitWebVRIfNecessary(JNIEnv* env, jobject obj) {
  if (!device_provider_)
    return false;

  device_provider_->OnGvrDelegateRemoved();
  return true;
}

bool VrShellDelegate::RequestWebVRPresent(
    device::GvrDeviceProvider* device_provider) {
  // Only set one device provider at a time
  DCHECK(!device_provider_);
  device_provider_ = device_provider;

  // If/When VRShell is ready for use it will call OnVrShellReady.
  JNIEnv* env = AttachCurrentThread();
  return Java_VrShellDelegate_enterVRIfNecessary(env,
                                                 j_vr_shell_delegate_.obj(),
                                                 true);
}

void VrShellDelegate::ExitWebVRPresent() {
  if (!device_provider_)
    return;

  device_provider_ = nullptr;

  // VRShell is no longer needed by WebVR, allow it to shut down if it's not
  // being used elsewhere.
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_exitWebVR(env, j_vr_shell_delegate_.obj());
}

void VrShellDelegate::OnVrShellReady(VrShell* vr_shell) {
  if (device_provider_)
    device_provider_->OnGvrDelegateReady(vr_shell);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

bool RegisterVrShellDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new VrShellDelegate(env, obj));
}

}  // namespace vr_shell

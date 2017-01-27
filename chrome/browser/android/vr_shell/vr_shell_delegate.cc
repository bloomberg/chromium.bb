// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/vr_shell/non_presenting_gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "jni/VrShellDelegate_jni.h"

using base::android::JavaParamRef;
using base::android::AttachCurrentThread;

namespace vr_shell {

VrShellDelegate::VrShellDelegate(JNIEnv* env, jobject obj)
    : weak_ptr_factory_(this) {
  j_vr_shell_delegate_.Reset(env, obj);
  GvrDelegateProvider::SetInstance(this);
}

VrShellDelegate::~VrShellDelegate() {
  GvrDelegateProvider::SetInstance(nullptr);
  if (device_provider_) {
    device_provider_->OnNonPresentingDelegateRemoved();
  }
}

VrShellDelegate* VrShellDelegate::GetNativeDelegate(
    JNIEnv* env, jobject jdelegate) {
  long native_delegate = Java_VrShellDelegate_getNativePointer(env, jdelegate);
  return reinterpret_cast<VrShellDelegate*>(native_delegate);
}

void VrShellDelegate::SetDelegate(device::GvrDelegate* delegate) {
  delegate_ = delegate;
  if (non_presenting_delegate_) {
    device::mojom::VRVSyncProviderRequest request =
        non_presenting_delegate_->OnSwitchToPresentingDelegate();
    if (request.is_pending())
      delegate->OnVRVsyncProviderRequest(std::move(request));
  }
  if (device_provider_) {
    device_provider_->OnGvrDelegateReady(delegate_);
  }
  delegate_->UpdateVSyncInterval(timebase_nanos_, interval_seconds_);
}

void VrShellDelegate::RemoveDelegate() {
  if (device_provider_) {
    device_provider_->OnGvrDelegateRemoved();
  }
  delegate_ = nullptr;
}

void VrShellDelegate::SetPresentResult(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean result) {
  CHECK(!present_callback_.is_null());
  present_callback_.Run(result);
  present_callback_.Reset();
}

void VrShellDelegate::DisplayActivate(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  if (device_provider_) {
    device_provider_->OnDisplayActivate();
  }
}

void VrShellDelegate::UpdateVSyncInterval(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jlong timebase_nanos,
                                          jdouble interval_seconds) {
  timebase_nanos_ = timebase_nanos;
  interval_seconds_ = interval_seconds;
  if (delegate_) {
    delegate_->UpdateVSyncInterval(timebase_nanos_,
                                   interval_seconds_);
  }
  if (non_presenting_delegate_) {
    non_presenting_delegate_->UpdateVSyncInterval(timebase_nanos_,
                                                  interval_seconds_);
  }
}

void VrShellDelegate::OnPause(JNIEnv* env,
                              const JavaParamRef<jobject>& obj) {
  if (non_presenting_delegate_) {
    non_presenting_delegate_->Pause();
  }
}

void VrShellDelegate::OnResume(JNIEnv* env,
                               const JavaParamRef<jobject>& obj) {
  if (non_presenting_delegate_) {
    non_presenting_delegate_->Resume();
  }
}

void VrShellDelegate::SetDeviceProvider(
    device::GvrDeviceProvider* device_provider) {
  device_provider_ = device_provider;
  if (device_provider_ && delegate_) {
    device_provider_->OnGvrDelegateReady(delegate_);
  }
}

void VrShellDelegate::RequestWebVRPresent(
    const base::Callback<void(bool)>& callback) {
  if (!present_callback_.is_null()) {
    // Can only handle one request at a time. This is also extremely unlikely to
    // happen in practice.
    callback.Run(false);
    return;
  }

  present_callback_ = std::move(callback);

  // If/When VRShell is ready for use it will call SetPresentResult.
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_presentRequested(env, j_vr_shell_delegate_.obj());
}

void VrShellDelegate::ExitWebVRPresent() {
  // VRShell is no longer needed by WebVR, allow it to shut down if it's not
  // being used elsewhere.
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_exitWebVR(env, j_vr_shell_delegate_.obj());
}

void VrShellDelegate::ForceExitVr() {
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_forceExitVr(env, j_vr_shell_delegate_.obj());
}

base::WeakPtr<VrShellDelegate> VrShellDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void VrShellDelegate::OnVRVsyncProviderRequest(
    device::mojom::VRVSyncProviderRequest request) {
  GetNonPresentingDelegate()->OnVRVsyncProviderRequest(std::move(request));
}

device::GvrDelegate* VrShellDelegate::GetNonPresentingDelegate() {
  if (!non_presenting_delegate_) {
    JNIEnv* env = AttachCurrentThread();
    jlong context = Java_VrShellDelegate_createNonPresentingNativeContext(
        env, j_vr_shell_delegate_.obj());
    if (!context)
      return nullptr;

    non_presenting_delegate_.reset(new NonPresentingGvrDelegate(context));
    non_presenting_delegate_->UpdateVSyncInterval(timebase_nanos_,
                                                  interval_seconds_);
  }
  return non_presenting_delegate_.get();
}

void VrShellDelegate::DestroyNonPresentingDelegate() {
  if (non_presenting_delegate_) {
    non_presenting_delegate_.reset(nullptr);
    JNIEnv* env = AttachCurrentThread();
    Java_VrShellDelegate_shutdownNonPresentingNativeContext(
        env, j_vr_shell_delegate_.obj());
  }
}

void VrShellDelegate::SetListeningForActivate(bool listening) {
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_setListeningForWebVrActivate(
      env, j_vr_shell_delegate_.obj(), listening);
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

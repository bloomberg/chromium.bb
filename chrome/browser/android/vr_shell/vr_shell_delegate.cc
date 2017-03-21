// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"

#include <utility>

#include "base/android/jni_android.h"
#include "chrome/browser/android/vr_shell/non_presenting_gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/android/gvr/gvr_gamepad_data_fetcher.h"
#include "jni/VrShellDelegate_jni.h"

using base::android::JavaParamRef;
using base::android::AttachCurrentThread;

namespace vr_shell {

VrShellDelegate::VrShellDelegate(JNIEnv* env, jobject obj) {
  j_vr_shell_delegate_.Reset(env, obj);
}

VrShellDelegate::~VrShellDelegate() {
  if (device_provider_) {
    device_provider_->Device()->OnDelegateChanged();
  }
}

device::GvrDelegateProvider* VrShellDelegate::CreateVrShellDelegate() {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jdelegate =
      Java_VrShellDelegate_getInstance(env);
  if (!jdelegate.is_null())
    return GetNativeVrShellDelegate(env, jdelegate.obj());
  return nullptr;
}

VrShellDelegate* VrShellDelegate::GetNativeVrShellDelegate(JNIEnv* env,
                                                           jobject jdelegate) {
  return reinterpret_cast<VrShellDelegate*>(
      Java_VrShellDelegate_getNativePointer(env, jdelegate));
}

void VrShellDelegate::SetDelegate(device::GvrDelegate* delegate,
                                  gvr_context* context) {
  delegate_ = delegate;
  // Clean up the non-presenting delegate.
  if (delegate_ && non_presenting_delegate_) {
    non_presenting_delegate_ = nullptr;
    JNIEnv* env = AttachCurrentThread();
    Java_VrShellDelegate_shutdownNonPresentingNativeContext(
        env, j_vr_shell_delegate_.obj());
  }
  if (device_provider_) {
    device::GvrDevice* device = device_provider_->Device();
    device::GamepadDataFetcherManager::GetInstance()->AddFactory(
        new device::GvrGamepadDataFetcher::Factory(context, device->id()));
    device->OnDelegateChanged();
  }

  delegate_->UpdateVSyncInterval(timebase_nanos_, interval_seconds_);
}

void VrShellDelegate::RemoveDelegate() {
  delegate_ = nullptr;
  device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      device::GAMEPAD_SOURCE_GVR);
  if (device_provider_) {
    CreateNonPresentingDelegate();
    device_provider_->Device()->OnDelegateChanged();
  }
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
    device_provider_->Device()->OnActivate(
        device::mojom::VRDisplayEventReason::MOUNTED);
  }
}

void VrShellDelegate::UpdateVSyncInterval(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jlong timebase_nanos,
                                          jdouble interval_seconds) {
  timebase_nanos_ = timebase_nanos;
  interval_seconds_ = interval_seconds;
  if (delegate_) {
    delegate_->UpdateVSyncInterval(timebase_nanos_, interval_seconds_);
  }
  if (non_presenting_delegate_) {
    non_presenting_delegate_->UpdateVSyncInterval(timebase_nanos_,
                                                  interval_seconds_);
  }
}

void VrShellDelegate::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (non_presenting_delegate_) {
    non_presenting_delegate_->Pause();
  }
}

void VrShellDelegate::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (non_presenting_delegate_) {
    non_presenting_delegate_->Resume();
  }
}

void VrShellDelegate::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

device::mojom::VRSubmitFrameClientPtr VrShellDelegate::TakeSubmitFrameClient() {
  return std::move(submit_client_);
}

void VrShellDelegate::SetDeviceProvider(
    device::GvrDeviceProvider* device_provider) {
  if (device_provider_ == device_provider)
    return;
  if (device_provider_)
    ClearDeviceProvider();
  CHECK(!device_provider_);
  device_provider_ = device_provider;
  if (!delegate_)
    CreateNonPresentingDelegate();
  device_provider_->Device()->OnDelegateChanged();
}

void VrShellDelegate::ClearDeviceProvider() {
  non_presenting_delegate_ = nullptr;
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_shutdownNonPresentingNativeContext(
      env, j_vr_shell_delegate_.obj());
  device_provider_ = nullptr;
}

void VrShellDelegate::RequestWebVRPresent(
    device::mojom::VRSubmitFrameClientPtr submit_client,
    const base::Callback<void(bool)>& callback) {
  if (!present_callback_.is_null()) {
    // Can only handle one request at a time. This is also extremely unlikely to
    // happen in practice.
    callback.Run(false);
    return;
  }

  present_callback_ = std::move(callback);
  submit_client_ = std::move(submit_client);

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

void VrShellDelegate::CreateNonPresentingDelegate() {
  JNIEnv* env = AttachCurrentThread();
  gvr_context* context = reinterpret_cast<gvr_context*>(
      Java_VrShellDelegate_createNonPresentingNativeContext(
          env, j_vr_shell_delegate_.obj()));
  non_presenting_delegate_ =
      base::MakeUnique<NonPresentingGvrDelegate>(context);
  non_presenting_delegate_->UpdateVSyncInterval(timebase_nanos_,
                                                interval_seconds_);
}

device::GvrDelegate* VrShellDelegate::GetDelegate() {
  if (delegate_)
    return delegate_;
  return non_presenting_delegate_.get();
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

static void OnLibraryAvailable(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  device::GvrDelegateProvider::SetInstance(
      base::Bind(&VrShellDelegate::CreateVrShellDelegate));
}

}  // namespace vr_shell

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/callback_helpers.h"
#include "chrome/browser/android/vr_shell/vr_metrics_util.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "jni/VrShellDelegate_jni.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"

using base::android::JavaParamRef;
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace vr_shell {

VrShellDelegate::VrShellDelegate(JNIEnv* env, jobject obj)
    : weak_ptr_factory_(this) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  j_vr_shell_delegate_.Reset(env, obj);
}

VrShellDelegate::~VrShellDelegate() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  if (device_provider_) {
    device_provider_->Device()->OnExitPresent();
  }
  if (!present_callback_.is_null()) {
    base::ResetAndReturn(&present_callback_).Run(false);
  }
}

device::GvrDelegateProvider* VrShellDelegate::CreateVrShellDelegate() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = Java_VrShellDelegate_getInstance(env);
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
                                  gvr::ViewerType viewer_type) {
  gvr_delegate_ = delegate;
  if (device_provider_) {
    device_provider_->Device()->OnDelegateChanged();
  }

  gvr_delegate_->UpdateVSyncInterval(timebase_nanos_, interval_seconds_);

  if (pending_successful_present_request_) {
    gvr_delegate_->ConnectPresentingService(
        std::move(submit_client_), std::move(presentation_provider_request_));
    base::ResetAndReturn(&present_callback_).Run(true);
    pending_successful_present_request_ = false;
  }
  JNIEnv* env = AttachCurrentThread();
  std::unique_ptr<VrCoreInfo> vr_core_info = MakeVrCoreInfo(env);
  VrMetricsUtil::LogGvrVersionForVrViewerType(viewer_type, *vr_core_info);
}

void VrShellDelegate::RemoveDelegate() {
  gvr_delegate_ = nullptr;
  if (device_provider_) {
    device_provider_->Device()->OnExitPresent();
    device_provider_->Device()->OnDelegateChanged();
  }
}

void VrShellDelegate::SetPresentResult(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean success) {
  CHECK(!present_callback_.is_null());
  if (success && !gvr_delegate_) {
    // We have to wait until the GL thread is ready since we have to pass it
    // the VRSubmitFrameClient.
    pending_successful_present_request_ = true;
    return;
  }

  if (success) {
    gvr_delegate_->ConnectPresentingService(
        std::move(submit_client_), std::move(presentation_provider_request_));
  }

  base::ResetAndReturn(&present_callback_).Run(success);
  pending_successful_present_request_ = false;
}

void VrShellDelegate::DisplayActivate(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  if (device_provider_) {
    device_provider_->Device()->OnActivate(
        device::mojom::VRDisplayEventReason::MOUNTED,
        base::Bind(&VrShellDelegate::OnActivateDisplayHandled,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void VrShellDelegate::UpdateVSyncInterval(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jlong timebase_nanos,
                                          jdouble interval_seconds) {
  timebase_nanos_ = timebase_nanos;
  interval_seconds_ = interval_seconds;
  if (gvr_delegate_) {
    gvr_delegate_->UpdateVSyncInterval(timebase_nanos_, interval_seconds_);
  }
}

void VrShellDelegate::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (gvr_api_) {
    gvr_api_->PauseTracking();
  }
}

void VrShellDelegate::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (gvr_api_) {
    gvr_api_->ResumeTracking();
  }
}

void VrShellDelegate::UpdateNonPresentingContext(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong context) {
  if (context == 0) {
    gvr_api_ = nullptr;
    return;
  }
  gvr_api_ = gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(context));
}

void VrShellDelegate::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void VrShellDelegate::SetDeviceProvider(
    device::GvrDeviceProvider* device_provider) {
  if (device_provider_ == device_provider)
    return;
  if (device_provider_)
    ClearDeviceProvider();
  CHECK(!device_provider_);
  device_provider_ = device_provider;
}

void VrShellDelegate::ClearDeviceProvider() {
  device_provider_ = nullptr;
}

void VrShellDelegate::RequestWebVRPresent(
    device::mojom::VRSubmitFrameClientPtr submit_client,
    device::mojom::VRPresentationProviderRequest request,
    const base::Callback<void(bool)>& callback) {
  if (!present_callback_.is_null()) {
    // Can only handle one request at a time. This is also extremely unlikely to
    // happen in practice.
    callback.Run(false);
    return;
  }

  present_callback_ = std::move(callback);
  submit_client_ = std::move(submit_client);
  presentation_provider_request_ = std::move(request);

  // If/When VRShell is ready for use it will call SetPresentResult.
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_presentRequested(env, j_vr_shell_delegate_.obj());
}

void VrShellDelegate::ExitWebVRPresent() {
  // VRShell is no longer needed by WebVR, allow it to shut down if it's not
  // being used elsewhere.
  JNIEnv* env = AttachCurrentThread();
  if (Java_VrShellDelegate_exitWebVRPresent(env, j_vr_shell_delegate_.obj())) {
    if (device_provider_) {
      device_provider_->Device()->OnExitPresent();
    }
  }
}

std::unique_ptr<VrCoreInfo> VrShellDelegate::MakeVrCoreInfo(JNIEnv* env) {
  return base::WrapUnique(reinterpret_cast<VrCoreInfo*>(
      Java_VrShellDelegate_getVrCoreInfo(env, j_vr_shell_delegate_.obj())));
}

void VrShellDelegate::OnActivateDisplayHandled(bool will_not_present) {
  if (will_not_present) {
    // WebVR page didn't request presentation in the vrdisplayactivate handler.
    // Tell VrShell that we are in VR Browsing Mode.
    ExitWebVRPresent();
  }
}

device::GvrDelegate* VrShellDelegate::GetDelegate() {
  return gvr_delegate_;
}

void VrShellDelegate::SetListeningForActivate(bool listening) {
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_setListeningForWebVrActivate(
      env, j_vr_shell_delegate_.obj(), listening);
}

void VrShellDelegate::GetNextMagicWindowPose(
    device::mojom::VRDisplay::GetNextMagicWindowPoseCallback callback) {
  if (!gvr_api_) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(
      device::GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_.get(), nullptr));
}

void VrShellDelegate::CreateVRDisplayInfo(
    const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
    uint32_t device_id) {
  if (gvr_delegate_) {
    gvr_delegate_->CreateVRDisplayInfo(callback, device_id);
    return;
  }
  // This is for magic window mode, which doesn't care what the render size is.
  callback.Run(device::GvrDelegate::CreateDefaultVRDisplayInfo(gvr_api_.get(),
                                                               device_id));
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

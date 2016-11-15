// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "jni/VrShellDelegate_jni.h"

using base::android::JavaParamRef;
using base::android::AttachCurrentThread;

namespace vr_shell {

// A non presenting delegate for magic window mode.
class GvrNonPresentingDelegate : public device::GvrDelegate {
 public:
  explicit GvrNonPresentingDelegate(jlong context) : weak_ptr_factory_(this) {
    gvr_api_ =
        gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(context));
  }

  virtual ~GvrNonPresentingDelegate() = default;

  // GvrDelegate implementation
  void SetWebVRSecureOrigin(bool secure_origin) override {}
  void SubmitWebVRFrame() override {}
  void UpdateWebVRTextureBounds(const gvr::Rectf& left_bounds,
                                const gvr::Rectf& right_bounds) override {}
  void SetGvrPoseForWebVr(const gvr::Mat4f& pose,
                          uint32_t pose_index) override {}
  gvr::GvrApi* gvr_api() override { return gvr_api_.get(); }
  base::WeakPtr<GvrNonPresentingDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
 private:
  std::unique_ptr<gvr::GvrApi> gvr_api_;
  base::WeakPtrFactory<GvrNonPresentingDelegate> weak_ptr_factory_;
};

VrShellDelegate::VrShellDelegate(JNIEnv* env, jobject obj)
    : device_provider_(nullptr) {
  j_vr_shell_delegate_.Reset(env, obj);
  GvrDelegateProvider::SetInstance(this);
}

VrShellDelegate::~VrShellDelegate() {
  GvrDelegateProvider::SetInstance(nullptr);
}

VrShellDelegate* VrShellDelegate::GetNativeDelegate(
    JNIEnv* env, jobject jdelegate) {
  long native_delegate = Java_VrShellDelegate_getNativePointer(env, jdelegate);
  return reinterpret_cast<VrShellDelegate*>(native_delegate);
}

base::WeakPtr<device::GvrDeviceProvider> VrShellDelegate::GetDeviceProvider() {
  return device_provider_;
}

void VrShellDelegate::ExitWebVRIfNecessary(JNIEnv* env, jobject obj) {
  if (!device_provider_)
    return;

  device_provider_->OnGvrDelegateRemoved();
}

bool VrShellDelegate::RequestWebVRPresent(
    base::WeakPtr<device::GvrDeviceProvider> device_provider) {
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
  device_provider_.reset();

  // VRShell is no longer needed by WebVR, allow it to shut down if it's not
  // being used elsewhere.
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_exitWebVR(env, j_vr_shell_delegate_.obj());
}

base::WeakPtr<device::GvrDelegate> VrShellDelegate::GetNonPresentingDelegate() {
  if (!non_presenting_delegate_) {
    JNIEnv* env = AttachCurrentThread();
    jlong context = Java_VrShellDelegate_createNonPresentingNativeContext(
        env, j_vr_shell_delegate_.obj());
    if (!context)
      return nullptr;

    non_presenting_delegate_.reset(new GvrNonPresentingDelegate(context));
  }
  return static_cast<GvrNonPresentingDelegate*>(non_presenting_delegate_.get())
      ->GetWeakPtr();
}

void VrShellDelegate::DestroyNonPresentingDelegate() {
  non_presenting_delegate_.reset(nullptr);
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_shutdownNonPresentingNativeContext(
      env, j_vr_shell_delegate_.obj());
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

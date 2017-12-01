// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/callback_helpers.h"
#include "chrome/browser/android/vr_shell/vr_metrics_util.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/vr_assets_component_installer.h"
#include "chrome/browser/vr/assets.h"
#include "chrome/browser/vr/metrics_helper.h"
#include "chrome/browser/vr/service/vr_device_manager.h"
#include "chrome/browser/vr/service/vr_service_impl.h"
#include "content/public/browser/webvr_service_provider.h"
#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "jni/VrShellDelegate_jni.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace vr_shell {

namespace {

class VrShellDelegateProviderFactory
    : public device::GvrDelegateProviderFactory {
 public:
  VrShellDelegateProviderFactory() = default;
  ~VrShellDelegateProviderFactory() override = default;
  device::GvrDelegateProvider* CreateGvrDelegateProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(VrShellDelegateProviderFactory);
};

device::GvrDelegateProvider*
VrShellDelegateProviderFactory::CreateGvrDelegateProvider() {
  return VrShellDelegate::CreateVrShellDelegate();
}

}  // namespace

VrShellDelegate::VrShellDelegate(JNIEnv* env, jobject obj)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  j_vr_shell_delegate_.Reset(env, obj);
}

VrShellDelegate::~VrShellDelegate() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  device::VRDevice* device = GetDevice();
  if (device)
    device->OnExitPresent();
  if (!present_callback_.is_null())
    base::ResetAndReturn(&present_callback_).Run(false);
}

device::GvrDelegateProvider* VrShellDelegate::CreateVrShellDelegate() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jdelegate = Java_VrShellDelegate_getInstance(env);
  if (!jdelegate.is_null())
    return GetNativeVrShellDelegate(env, jdelegate);
  return nullptr;
}

VrShellDelegate* VrShellDelegate::GetNativeVrShellDelegate(
    JNIEnv* env,
    const JavaRef<jobject>& jdelegate) {
  return reinterpret_cast<VrShellDelegate*>(
      Java_VrShellDelegate_getNativePointer(env, jdelegate));
}

void VrShellDelegate::SetDelegate(VrShell* vr_shell,
                                  gvr::ViewerType viewer_type) {
  vr_shell_ = vr_shell;
  device::VRDevice* device = GetDevice();
  // When VrShell is created, we disable magic window mode as the user is inside
  // the headset. As currently implemented, orientation-based magic window
  // doesn't make sense when the window is fixed and the user is moving.
  if (device)
    device->SetMagicWindowEnabled(false);

  if (pending_successful_present_request_) {
    CHECK(!present_callback_.is_null());
    base::ResetAndReturn(&present_callback_).Run(true);
  }
  JNIEnv* env = AttachCurrentThread();
  std::unique_ptr<VrCoreInfo> vr_core_info = MakeVrCoreInfo(env);
  VrMetricsUtil::LogGvrVersionForVrViewerType(viewer_type, *vr_core_info);
}

void VrShellDelegate::RemoveDelegate() {
  vr_shell_ = nullptr;
  device::VRDevice* device = GetDevice();
  if (device) {
    device->SetMagicWindowEnabled(true);
    device->OnExitPresent();
  }
}

void VrShellDelegate::SetPresentResult(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean success) {
  CHECK(!present_callback_.is_null());
  base::ResetAndReturn(&present_callback_).Run(static_cast<bool>(success));
}

void VrShellDelegate::OnPresentResult(
    device::mojom::VRSubmitFrameClientPtr submit_client,
    device::mojom::VRPresentationProviderRequest request,
    device::mojom::VRDisplayInfoPtr display_info,
    base::Callback<void(bool)> callback,
    bool success) {
  if (!success) {
    std::move(callback).Run(false);
    return;
  }

  if (!vr_shell_) {
    // We have to wait until the GL thread is ready since we have to pass it
    // the VRSubmitFrameClient.
    pending_successful_present_request_ = true;
    present_callback_ =
        base::Bind(&VrShellDelegate::OnPresentResult, base::Unretained(this),
                   base::Passed(&submit_client), base::Passed(&request),
                   base::Passed(&display_info), base::Passed(&callback));
    return;
  }

  vr_shell_->ConnectPresentingService(
      std::move(submit_client), std::move(request), std::move(display_info));

  std::move(callback).Run(true);
  pending_successful_present_request_ = false;
}

void VrShellDelegate::DisplayActivate(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  device::GvrDevice* device = static_cast<device::GvrDevice*>(GetDevice());
  if (device) {
    device->Activate(device::mojom::VRDisplayEventReason::MOUNTED,
                     base::Bind(&VrShellDelegate::OnActivateDisplayHandled,
                                weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnActivateDisplayHandled(true /* will_not_present */);
  }
}

void VrShellDelegate::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (vr_shell_)
    return;
  device::VRDevice* device = GetDevice();
  if (device)
    device->PauseTracking();
}

void VrShellDelegate::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (vr_shell_)
    return;
  device::VRDevice* device = GetDevice();
  if (device)
    device->ResumeTracking();
}

bool VrShellDelegate::IsClearActivatePending(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  return !clear_activate_task_.IsCancelled();
}

void VrShellDelegate::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void VrShellDelegate::SetDeviceId(unsigned int device_id) {
  device_id_ = device_id;
  if (vr_shell_) {
    device::VRDevice* device = GetDevice();
    // See comment in VrShellDelegate::SetDelegate. This handles the case where
    // VrShell is created before the device/ code is initialized (like when
    // entering VR browsing on a non-webVR page).
    if (device)
      device->SetMagicWindowEnabled(false);
  }
}

void VrShellDelegate::RequestWebVRPresent(
    device::mojom::VRSubmitFrameClientPtr submit_client,
    device::mojom::VRPresentationProviderRequest request,
    device::mojom::VRDisplayInfoPtr display_info,
    base::Callback<void(bool)> callback) {
  if (!present_callback_.is_null()) {
    // Can only handle one request at a time. This is also extremely unlikely to
    // happen in practice.
    std::move(callback).Run(false);
    return;
  }
  present_callback_ =
      base::Bind(&VrShellDelegate::OnPresentResult, base::Unretained(this),
                 base::Passed(&submit_client), base::Passed(&request),
                 base::Passed(&display_info), base::Passed(&callback));

  // If/When VRShell is ready for use it will call SetPresentResult.
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_presentRequested(env, j_vr_shell_delegate_);
}

void VrShellDelegate::ExitWebVRPresent() {
  // VRShell is no longer needed by WebVR, allow it to shut down if it's not
  // being used elsewhere.
  JNIEnv* env = AttachCurrentThread();
  if (Java_VrShellDelegate_exitWebVRPresent(env, j_vr_shell_delegate_)) {
    device::VRDevice* device = GetDevice();
    if (device)
      device->OnExitPresent();
  }
}

std::unique_ptr<VrCoreInfo> VrShellDelegate::MakeVrCoreInfo(JNIEnv* env) {
  return std::unique_ptr<VrCoreInfo>(reinterpret_cast<VrCoreInfo*>(
      Java_VrShellDelegate_getVrCoreInfo(env, j_vr_shell_delegate_)));
}

void VrShellDelegate::OnActivateDisplayHandled(bool will_not_present) {
  if (will_not_present) {
    // WebVR page didn't request presentation in the vrdisplayactivate handler.
    // Tell VrShell that we are in VR Browsing Mode.
    ExitWebVRPresent();
  }
}

void VrShellDelegate::OnListeningForActivateChanged(bool listening) {
  if (listening) {
    SetListeningForActivate(true);
  } else {
    // We post here to ensure that this runs after Android finishes running all
    // onPause handlers. This allows us to capture the pre-paused state during
    // onPause in java, so we know that the pause is the cause of the focus
    // loss, and that the page is still listening for activate.
    clear_activate_task_.Reset(
        base::Bind(&VrShellDelegate::SetListeningForActivate,
                   weak_ptr_factory_.GetWeakPtr(), false));
    task_runner_->PostTask(FROM_HERE, clear_activate_task_.callback());
  }
}

void VrShellDelegate::SetListeningForActivate(bool listening) {
  clear_activate_task_.Cancel();
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_setListeningForWebVrActivate(env, j_vr_shell_delegate_,
                                                    listening);
}

device::VRDevice* VrShellDelegate::GetDevice() {
  return vr::VRDeviceManager::GetInstance()->GetDevice(device_id_);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_VrShellDelegate_Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new VrShellDelegate(env, obj));
}

static void JNI_VrShellDelegate_OnLibraryAvailable(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  device::GvrDelegateProviderFactory::Install(
      new VrShellDelegateProviderFactory);
}

static void JNI_VrShellDelegate_RegisterVrAssetsComponent(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  component_updater::RegisterVrAssetsComponent(
      g_browser_process->component_updater());
}

static void JNI_VrShellDelegate_OnChromeStarted(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  vr::Assets::GetInstance()->GetMetricsHelper()->OnChromeStarted();
}

}  // namespace vr_shell

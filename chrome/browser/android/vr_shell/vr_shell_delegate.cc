// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/vr_metrics_util.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/origin_util.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_manager.h"
#include "device/vr/vr_display_impl.h"
#include "jni/VrShellDelegate_jni.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace vr_shell {

namespace {

content::RenderFrameHost* GetHostForDisplay(device::VRDisplayImpl* display) {
  return content::RenderFrameHost::FromID(display->ProcessId(),
                                          display->RoutingId());
}

bool IsSecureContext(content::RenderFrameHost* host) {
  DCHECK(host);
  while (host != nullptr) {
    if (!content::IsOriginSecure(host->GetLastCommittedURL()))
      return false;
    host = host->GetParent();
  }
  return true;
}

}  // namespace

class DelegateWebContentsObserver : public content::WebContentsObserver {
 public:
  DelegateWebContentsObserver(VrShellDelegate* delegate,
                              content::WebContents* contents)
      : content::WebContentsObserver(contents), delegate_(delegate) {}

 private:
  void OnWebContentsFocused(content::RenderWidgetHost* host) override {
    delegate_->OnWebContentsFocused(host);
  }
  void OnWebContentsLostFocus(content::RenderWidgetHost* host) override {
    delegate_->OnWebContentsLostFocus(host);
  }

  VrShellDelegate* delegate_;
};

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
  if (device)
    device->OnChanged();

  if (pending_successful_present_request_) {
    SetPresentResult(true);
  }
  JNIEnv* env = AttachCurrentThread();
  std::unique_ptr<VrCoreInfo> vr_core_info = MakeVrCoreInfo(env);
  VrMetricsUtil::LogGvrVersionForVrViewerType(viewer_type, *vr_core_info);
}

void VrShellDelegate::RemoveDelegate() {
  vr_shell_ = nullptr;
  device::VRDevice* device = GetDevice();
  if (device) {
    device->OnExitPresent();
    device->OnChanged();
  }
}

void VrShellDelegate::SetPresentResult(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean success) {
  SetPresentResult(static_cast<bool>(success));
}

void VrShellDelegate::SetPresentResult(bool success) {
  CHECK(!present_callback_.is_null());
  if (!success) {
    pending_successful_present_request_ = false;
    base::ResetAndReturn(&present_callback_).Run(false);
    return;
  }

  if (!vr_shell_) {
    // We have to wait until the GL thread is ready since we have to pass it
    // the VRSubmitFrameClient.
    pending_successful_present_request_ = true;
    return;
  }

  vr_shell_->ConnectPresentingService(
      std::move(submit_client_), std::move(presentation_provider_request_));

  base::ResetAndReturn(&present_callback_).Run(true);
  pending_successful_present_request_ = false;

  device::VRDevice* device = GetDevice();
  if (!device) {
    ExitWebVRPresent();
    return;
  }
  device::VRDisplayImpl* presenting_display = device->GetPresentingDisplay();
  if (!presenting_display) {
    ExitWebVRPresent();
    return;
  }
  content::RenderFrameHost* host = GetHostForDisplay(presenting_display);
  if (!host) {
    ExitWebVRPresent();
    return;
  }
  vr_shell_->SetWebVRSecureOrigin(IsSecureContext(host));
}

void VrShellDelegate::DisplayActivate(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  if (activatable_display_) {
    activatable_display_->OnActivate(
        device::mojom::VRDisplayEventReason::MOUNTED,
        base::Bind(&VrShellDelegate::OnActivateDisplayHandled,
                   weak_ptr_factory_.GetWeakPtr()));
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

bool VrShellDelegate::IsClearActivatePending(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  return !clear_activate_task_.IsCancelled();
}

void VrShellDelegate::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void VrShellDelegate::SetDeviceId(unsigned int device_id) {
  device_id_ = device_id;
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
  return base::WrapUnique(reinterpret_cast<VrCoreInfo*>(
      Java_VrShellDelegate_getVrCoreInfo(env, j_vr_shell_delegate_)));
}

void VrShellDelegate::OnActivateDisplayHandled(bool will_not_present) {
  if (will_not_present) {
    // WebVR page didn't request presentation in the vrdisplayactivate handler.
    // Tell VrShell that we are in VR Browsing Mode.
    ExitWebVRPresent();
  }
}

void VrShellDelegate::OnDisplayAdded(device::VRDisplayImpl* display) {
  content::RenderFrameHost* host = GetHostForDisplay(display);
  if (host == nullptr)
    return;
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(host);
  CHECK(web_contents);
  content::RenderWidgetHost* render_widget_host =
      host->GetView()->GetRenderWidgetHost();
  displays_.emplace(render_widget_host, display);
  observers_.emplace(display, base::MakeUnique<DelegateWebContentsObserver>(
                                  this, web_contents));
  if (host->GetView()->HasFocus())
    OnWebContentsFocused(render_widget_host);
}

void VrShellDelegate::OnDisplayRemoved(device::VRDisplayImpl* display) {
  if (activatable_display_ == display) {
    SetListeningForActivate(false);
    activatable_display_ = nullptr;
  }
  for (auto it = displays_.begin(); it != displays_.end();) {
    if (it->second == display) {
      it = displays_.erase(it);
    } else {
      ++it;
    }
  }
  auto it = observers_.find(display);
  if (it != observers_.end())
    observers_.erase(it);
}

void VrShellDelegate::OnListeningForActivateChanged(
    device::VRDisplayImpl* display) {
  content::RenderFrameHost* host = GetHostForDisplay(display);
  bool has_focus = host != nullptr && host->GetView()->HasFocus();
  if (display->ListeningForActivate() && has_focus) {
    OnFocusedAndActivatable(display);
  } else {
    if (activatable_display_ != display)
      return;
    OnLostFocusedAndActivatable();
  }
}

void VrShellDelegate::OnWebContentsFocused(content::RenderWidgetHost* host) {
  auto it = displays_.find(host);
  if (it == displays_.end())
    return;
  if (!it->second->ListeningForActivate())
    return;
  OnFocusedAndActivatable(it->second);
}

void VrShellDelegate::OnWebContentsLostFocus(content::RenderWidgetHost* host) {
  auto it = displays_.find(host);
  if (it == displays_.end())
    return;
  if (activatable_display_ != it->second)
    return;
  if (!it->second->ListeningForActivate())
    return;
  OnLostFocusedAndActivatable();
}

void VrShellDelegate::OnFocusedAndActivatable(device::VRDisplayImpl* display) {
  activatable_display_ = display;
  SetListeningForActivate(true);
  clear_activate_task_.Cancel();
}

void VrShellDelegate::OnLostFocusedAndActivatable() {
  // We post here to ensure that this runs after Android finishes running all
  // onPause handlers. This allows us to capture the pre-paused state during
  // onPause in java, so we know that the pause is the cause of the focus loss,
  // and that the page is still listening for activate.
  clear_activate_task_.Reset(
      base::Bind(&VrShellDelegate::SetListeningForActivate,
                 weak_ptr_factory_.GetWeakPtr(), false));
  task_runner_->PostTask(FROM_HERE, clear_activate_task_.callback());
}

void VrShellDelegate::SetListeningForActivate(bool listening) {
  clear_activate_task_.Cancel();
  JNIEnv* env = AttachCurrentThread();
  Java_VrShellDelegate_setListeningForWebVrActivate(env, j_vr_shell_delegate_,
                                                    listening);
}

void VrShellDelegate::GetNextMagicWindowPose(
    device::VRDisplayImpl* display,
    device::mojom::VRDisplay::GetNextMagicWindowPoseCallback callback) {
  content::RenderFrameHost* host = GetHostForDisplay(display);
  if (!gvr_api_ || vr_shell_ || host == nullptr ||
      !host->GetView()->HasFocus()) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(
      device::GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_.get(), nullptr));
}

void VrShellDelegate::CreateVRDisplayInfo(
    const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
    uint32_t device_id) {
  if (vr_shell_) {
    vr_shell_->CreateVRDisplayInfo(callback, device_id);
    return;
  }
  // This is for magic window mode, which doesn't care what the render size is.
  callback.Run(device::GvrDelegate::CreateDefaultVRDisplayInfo(gvr_api_.get(),
                                                               device_id));
}

device::VRDevice* VrShellDelegate::GetDevice() {
  return device::VRDeviceManager::GetInstance()->GetDevice(device_id_);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new VrShellDelegate(env, obj));
}

static void OnLibraryAvailable(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  device::GvrDelegateProvider::SetInstance(
      base::Bind(&VrShellDelegate::CreateVrShellDelegate));
}

}  // namespace vr_shell

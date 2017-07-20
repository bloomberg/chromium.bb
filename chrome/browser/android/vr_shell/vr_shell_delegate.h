// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_DELEGATE_H_

#include <jni.h>

#include <map>
#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "chrome/browser/android/vr_shell/vr_core_info.h"
#include "chrome/browser/android/vr_shell/vr_usage_monitor.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace content {
class RenderWidgetHost;
}

namespace device {
class GvrDelegate;
class GvrDeviceProvider;
}

namespace vr_shell {

class DelegateWebContentsObserver;

class VrShellDelegate : public device::GvrDelegateProvider {
 public:
  VrShellDelegate(JNIEnv* env, jobject obj);
  ~VrShellDelegate() override;

  static device::GvrDelegateProvider* CreateVrShellDelegate();

  static VrShellDelegate* GetNativeVrShellDelegate(JNIEnv* env,
                                                   jobject jdelegate);

  void SetDelegate(device::GvrDelegate* delegate, gvr::ViewerType viewer_type);
  void RemoveDelegate();

  void SetPresentResult(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jboolean success);
  void DisplayActivate(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);
  void UpdateVSyncInterval(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jlong timebase_nanos,
                           jlong interval_micros);
  void OnPause(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void UpdateNonPresentingContext(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong context);
  bool IsClearActivatePending(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  device::GvrDeviceProvider* device_provider() { return device_provider_; }

  void OnWebContentsFocused(content::RenderWidgetHost* host);
  void OnWebContentsLostFocus(content::RenderWidgetHost* host);

  // device::GvrDelegateProvider implementation.
  void ExitWebVRPresent() override;
  void CreateVRDisplayInfo(
      const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
      uint32_t device_id) override;

 private:
  // device::GvrDelegateProvider implementation.
  void SetDeviceProvider(device::GvrDeviceProvider* device_provider) override;
  void ClearDeviceProvider() override;
  void RequestWebVRPresent(device::mojom::VRSubmitFrameClientPtr submit_client,
                           device::mojom::VRPresentationProviderRequest request,
                           const base::Callback<void(bool)>& callback) override;
  device::GvrDelegate* GetDelegate() override;
  void OnDisplayAdded(device::VRDisplayImpl* display) override;
  void OnDisplayRemoved(device::VRDisplayImpl* display) override;
  void OnListeningForActivateChanged(device::VRDisplayImpl* display) override;
  void GetNextMagicWindowPose(
      device::VRDisplayImpl* display,
      device::mojom::VRDisplay::GetNextMagicWindowPoseCallback callback)
      override;

  void OnActivateDisplayHandled(bool will_not_present);
  void OnFocusedAndActivatable(device::VRDisplayImpl* display);
  void OnLostFocusedAndActivatable();
  void SetListeningForActivate(bool listening);

  std::unique_ptr<VrCoreInfo> MakeVrCoreInfo(JNIEnv* env);

  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_delegate_;
  device::GvrDeviceProvider* device_provider_ = nullptr;
  device::GvrDelegate* gvr_delegate_ = nullptr;
  base::Callback<void(bool)> present_callback_;
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;
  device::mojom::VRSubmitFrameClientPtr submit_client_;
  device::mojom::VRPresentationProviderRequest presentation_provider_request_;
  bool pending_successful_present_request_ = false;
  std::unique_ptr<gvr::GvrApi> gvr_api_;

  std::map<content::RenderWidgetHost*, device::VRDisplayImpl*> displays_;
  std::map<device::VRDisplayImpl*, std::unique_ptr<DelegateWebContentsObserver>>
      observers_;
  device::VRDisplayImpl* activatable_display_ = nullptr;
  base::CancelableClosure clear_activate_task_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<VrShellDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShellDelegate);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_DELEGATE_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace base {
class ListValue;
}

namespace blink {
class WebInputEvent;
}

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

namespace vr_shell {

class AndroidUiGestureTarget;
class UiInterface;
class VrCompositor;
class VrGLThread;
class VrInputManager;
class VrMetricsHelper;
class VrShellDelegate;
class VrWebContentsObserver;

enum UiAction {
  HISTORY_BACK = 0,
  HISTORY_FORWARD,
  RELOAD,
  ZOOM_OUT,
  ZOOM_IN,
  RELOAD_UI,
  LOAD_URL,
  OMNIBOX_CONTENT,
  SET_CONTENT_PAUSED,
  SHOW_TAB,
  OPEN_NEW_TAB,
  KEY_EVENT,
};

class VrMetricsHelper;

// The native instance of the Java VrShell. This class is not threadsafe and
// must only be used on the UI thread.
class VrShell : public device::GvrDelegate, content::WebContentsObserver {
 public:
  VrShell(JNIEnv* env,
          jobject obj,
          ui::WindowAndroid* content_window,
          content::WebContents* ui_contents,
          ui::WindowAndroid* ui_window,
          bool for_web_vr,
          VrShellDelegate* delegate,
          gvr_context* gvr_api,
          bool reprojected_rendering);
  void SwapContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& web_contents,
      const base::android::JavaParamRef<jobject>& touch_event_synthesizer);
  void LoadUIContent(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnTriggerEvent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnPause(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetSurface(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jobject>& surface);
  void SetWebVrMode(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    bool enabled);
  void OnLoadProgressChanged(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             double progress);
  void OnTabListCreated(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jobjectArray tabs,
                        jobjectArray incognito_tabs);
  void OnTabUpdated(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jboolean incognito,
                    jint id,
                    jstring jtitle);
  void OnTabRemoved(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jboolean incognito,
                    jint id);
  base::android::ScopedJavaGlobalRef<jobject> TakeContentSurface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void RestoreContentSurface(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  void ContentWebContentsDestroyed();
  // Called when our WebContents have been hidden. Usually a sign that something
  // like another tab placed in front of it.
  void ContentWasHidden();
  void ContentWasShown();

  // html/js UI hooks.
  static base::WeakPtr<VrShell> GetWeakPtr(
      const content::WebContents* web_contents);

  UiInterface* GetUiInterface();
  void OnDomContentsLoaded();

  void UiSurfaceChanged(jobject surface);
  void ContentSurfaceChanged(jobject surface);
  void GvrDelegateReady();
  void AppButtonPressed();

  void ContentPhysicalBoundsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint width,
      jint height,
      jfloat dpr);

  void UIPhysicalBoundsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint width,
      jint height,
      jfloat dpr);

  void UpdateScene(const base::ListValue* args);

  // Perform a UI action triggered by the javascript API.
  void DoUiAction(const UiAction action,
                  const base::DictionaryValue* arguments);

  void SetContentCssSize(float width, float height, float dpr);
  void SetUiCssSize(float width, float height, float dpr);

  void ContentFrameWasResized(bool width_changed);

  void ForceExitVr();

  void ProcessUIGesture(std::unique_ptr<blink::WebInputEvent> event);
  void ProcessContentGesture(std::unique_ptr<blink::WebInputEvent> event);

  // TODO(mthiesse): Find a better place for these functions to live.
  static device::mojom::VRPosePtr VRPosePtrFromGvrPose(gvr::Mat4f head_mat);
  static device::mojom::VRDisplayInfoPtr CreateVRDisplayInfo(
      gvr::GvrApi* gvr_api,
      gvr::Sizei compositor_size,
      uint32_t device_id);

 private:
  ~VrShell() override;
  void PostToGlThreadWhenReady(const base::Closure& task);
  void SetContentPaused(bool paused);
  void SetUiState();

  // content::WebContentsObserver implementation.
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void MainFrameWasResized(bool width_changed) override;
  void WebContentsDestroyed() override;

  // device::GvrDelegate implementation
  void SetWebVRSecureOrigin(bool secure_origin) override;
  void SubmitWebVRFrame() override;
  void UpdateWebVRTextureBounds(int16_t frame_index,
                                const gvr::Rectf& left_bounds,
                                const gvr::Rectf& right_bounds) override;
  void OnVRVsyncProviderRequest(
      device::mojom::VRVSyncProviderRequest request) override;
  void UpdateVSyncInterval(int64_t timebase_nanos,
                           double interval_seconds) override;
  bool SupportsPresentation() override;
  void ResetPose() override;
  void CreateVRDisplayInfo(
      const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
      uint32_t device_id) override;

  void ProcessTabArray(JNIEnv* env, jobjectArray tabs, bool incognito);

  bool vr_shell_enabled_;

  std::unique_ptr<UiInterface> html_interface_;
  bool content_paused_ = false;
  bool webvr_mode_ = false;

  content::WebContents* main_contents_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> j_motion_event_synthesizer_;
  ui::WindowAndroid* content_window_;
  std::unique_ptr<VrCompositor> content_compositor_;
  content::WebContents* ui_contents_;
  std::unique_ptr<VrCompositor> ui_compositor_;

  std::unique_ptr<VrWebContentsObserver> vr_web_contents_observer_;

  VrShellDelegate* delegate_provider_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_;

  std::unique_ptr<VrInputManager> content_input_manager_;
  std::unique_ptr<AndroidUiGestureTarget> android_ui_gesture_target_;
  std::unique_ptr<VrInputManager> ui_input_manager_;
  std::unique_ptr<VrMetricsHelper> metrics_helper_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<VrGLThread> gl_thread_;
  bool reprojected_rendering_;

  jobject content_surface_ = nullptr;

  // TODO(mthiesse): Remove the need for this to be stored here.
  // crbug.com/674594
  gvr_context* gvr_api_;

  base::WeakPtrFactory<VrShell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShell);
};

bool RegisterVrShell(JNIEnv* env);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_

#include <jni.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/ui/toolbar/chrome_toolbar_model_delegate.h"
#include "chrome/browser/vr/exit_vr_prompt_choice.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/geolocation/public/interfaces/geolocation_config.mojom.h"
#include "device/vr/android/gvr/cardboard_gamepad_data_provider.h"
#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "device/vr/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace vr {
class BrowserUiInterface;
class ToolbarHelper;
class WebContentsEventForwarder;
}  // namespace vr

namespace vr_shell {

class AndroidUiGestureTarget;
class VrCompositor;
class VrGLThread;
class VrMetricsHelper;
class VrShellDelegate;
class VrWebContentsObserver;

enum UiAction {
  HISTORY_BACK = 0,
  HISTORY_FORWARD,
  RELOAD,
  SHOW_TAB,
  OPEN_NEW_TAB,
  EXIT_PRESENT,
};

class VrMetricsHelper;

// The native instance of the Java VrShell. This class is not threadsafe and
// must only be used on the UI thread.
class VrShell : device::GvrGamepadDataProvider,
                device::CardboardGamepadDataProvider,
                public ChromeToolbarModelDelegate {
 public:
  VrShell(JNIEnv* env,
          const base::android::JavaParamRef<jobject>& obj,
          ui::WindowAndroid* window,
          bool for_web_vr,
          bool web_vr_autopresentation_expected,
          bool in_cct,
          VrShellDelegate* delegate,
          gvr_context* gvr_api,
          bool reprojected_rendering,
          float display_width_meters,
          float display_height_meters,
          int display_width_pixels,
          int display_height_pixels);
  void SwapContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& web_contents,
      const base::android::JavaParamRef<jobject>& android_ui_gesture_target);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnTriggerEvent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      bool touched);
  void OnPause(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetSurface(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jobject>& surface);
  void SetWebVrMode(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    bool enabled,
                    bool show_toast);
  bool GetWebVrMode(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  bool IsDisplayingUrlForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void OnFullscreenChanged(bool enabled);
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
  void OnContentPaused(bool paused);
  void NavigateBack();
  void ExitCct();
  void ToggleCardboardGamepad(bool enabled);
  void ToggleGvrGamepad(bool enabled);
  base::android::ScopedJavaGlobalRef<jobject> TakeContentSurface(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void RestoreContentSurface(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void SetHistoryButtonsEnabled(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                jboolean can_go_back,
                                jboolean can_go_forward);
  void RequestToExitVr(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       int reason);
  void LogUnsupportedModeUserMetric(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      int mode);

  void ContentWebContentsDestroyed();
  // Called when our WebContents have been hidden. Usually a sign that something
  // like another tab placed in front of it.
  void ContentWasHidden();
  void ContentWasShown();

  void ContentSurfaceChanged(jobject surface);
  void GvrDelegateReady(gvr::ViewerType viewer_type);

  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height);
  void ContentPhysicalBoundsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint width,
      jint height,
      jfloat dpr);

  // Perform a UI action triggered by the javascript API.
  void DoUiAction(const UiAction action,
                  const base::DictionaryValue* arguments);

  void SetHighAccuracyLocation(bool high_accuracy_location);
  void SetContentCssSize(float width, float height, float dpr);

  void ContentFrameWasResized(bool width_changed);

  void ForceExitVr();
  void ExitPresent();
  void ExitFullscreen();
  void LogUnsupportedModeUserMetric(vr::UiUnsupportedMode mode);
  void OnUnsupportedMode(vr::UiUnsupportedMode mode);
  void OnExitVrPromptResult(vr::UiUnsupportedMode reason,
                            vr::ExitVrPromptChoice choice);
  void OnContentScreenBoundsChanged(const gfx::SizeF& bounds);

  void ProcessContentGesture(std::unique_ptr<blink::WebInputEvent> event,
                             int content_id);

  void ConnectPresentingService(
      device::mojom::VRSubmitFrameClientPtr submit_client,
      device::mojom::VRPresentationProviderRequest request,
      device::mojom::VRDisplayInfoPtr display_info);

  // device::GvrGamepadDataProvider implementation.
  void UpdateGamepadData(device::GvrGamepadData) override;
  void RegisterGvrGamepadDataFetcher(device::GvrGamepadDataFetcher*) override;

  // device::CardboardGamepadDataProvider implementation.
  void RegisterCardboardGamepadDataFetcher(
      device::CardboardGamepadDataFetcher*) override;

  // ChromeToolbarModelDelegate implementation.
  content::WebContents* GetActiveWebContents() const override;
  bool ShouldDisplayURL() const override;

 private:
  ~VrShell() override;
  void PostToGlThread(const base::Location& from_here,
                      const base::Closure& task);
  void SetUiState();

  void ProcessTabArray(JNIEnv* env, jobjectArray tabs, bool incognito);

  void PollMediaAccessFlag();

  bool HasDaydreamSupport(JNIEnv* env);

  void ExitVrDueToUnsupportedMode(vr::UiUnsupportedMode mode);

  content::WebContents* GetNonNativePageWebContents() const;

  bool vr_shell_enabled_;

  bool webvr_mode_ = false;

  content::WebContents* web_contents_ = nullptr;
  bool web_contents_is_native_page_ = false;
  base::android::ScopedJavaGlobalRef<jobject> j_motion_event_synthesizer_;
  ui::WindowAndroid* window_;
  std::unique_ptr<VrCompositor> compositor_;

  std::unique_ptr<VrWebContentsObserver> vr_web_contents_observer_;

  VrShellDelegate* delegate_provider_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_;

  std::unique_ptr<vr::WebContentsEventForwarder> web_contents_event_forwarder_;
  std::unique_ptr<AndroidUiGestureTarget> android_ui_gesture_target_;
  std::unique_ptr<VrMetricsHelper> metrics_helper_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<VrGLThread> gl_thread_;
  bool reprojected_rendering_;

  vr::BrowserUiInterface* ui_;
  std::unique_ptr<vr::ToolbarHelper> toolbar_;

  device::mojom::GeolocationConfigPtr geolocation_config_;

  jobject content_surface_ = nullptr;
  bool taken_surface_ = false;
  base::CancelableClosure poll_capturing_media_task_;
  bool is_capturing_audio_ = false;
  bool is_capturing_video_ = false;
  bool is_capturing_screen_ = false;
  bool is_bluetooth_connected_ = false;
  bool high_accuracy_location_ = false;

  // Are we currently providing a gamepad factory to the gamepad manager?
  bool gvr_gamepad_source_active_ = false;
  bool cardboard_gamepad_source_active_ = false;
  bool pending_cardboard_trigger_ = false;

  // Registered fetchers, must remain alive for UpdateGamepadData calls.
  // That's ok since the fetcher is only destroyed from VrShell's destructor.
  device::GvrGamepadDataFetcher* gvr_gamepad_data_fetcher_ = nullptr;
  device::CardboardGamepadDataFetcher* cardboard_gamepad_data_fetcher_ =
      nullptr;
  int64_t cardboard_gamepad_timer_ = 0;

  // Content id
  int content_id_ = 0;

  gfx::SizeF display_size_meters_;
  gfx::Size display_size_pixels_;

  base::WeakPtrFactory<VrShell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShell);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_

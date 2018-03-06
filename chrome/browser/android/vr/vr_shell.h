// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_VR_SHELL_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/toolbar/chrome_toolbar_model_delegate.h"
#include "chrome/browser/vr/assets_load_status.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/exit_vr_prompt_choice.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/android/gvr/cardboard_gamepad_data_provider.h"
#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "services/device/public/mojom/geolocation_config.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace base {
class Version;
}  // namespace base

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace gl {
class SurfaceTexture;
}

namespace vr {

class BrowserUiInterface;
class AndroidUiGestureTarget;
class AutocompleteController;
class ToolbarHelper;
class VrGLThread;
class VrInputConnection;
class VrMetricsHelper;
class VrShellDelegate;
class VrWebContentsObserver;
struct AutocompleteRequest;

// The native instance of the Java VrShell. This class is not threadsafe and
// must only be used on the UI thread.
class VrShell : device::GvrGamepadDataProvider,
                device::CardboardGamepadDataProvider,
                VoiceResultDelegate,
                public ChromeToolbarModelDelegate {
 public:
  VrShell(JNIEnv* env,
          const base::android::JavaParamRef<jobject>& obj,
          const UiInitialState& ui_initial_state,
          VrShellDelegate* delegate,
          gvr_context* gvr_api,
          bool reprojected_rendering,
          float display_width_meters,
          float display_height_meters,
          int display_width_pixels,
          int display_height_pixels,
          bool pause_content);
  void SwapContents(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jobject>& web_contents);
  void SetAndroidGestureTarget(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& android_ui_gesture_target);
  void SetDialogGestureTarget(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& dialog_gesture_target);
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
  void Navigate(GURL url);
  void NavigateBack();
  void ExitCct();
  void CloseHostedDialog();
  void ToggleCardboardGamepad(bool enabled);
  void ToggleGvrGamepad(bool enabled);
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

  void ShowSoftInput(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     bool show);
  void UpdateWebInputIndices(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             int selection_start,
                             int selection_end,
                             int composition_start,
                             int composition_end);
  void ResumeContentRendering(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  void ContentWebContentsDestroyed();

  void ContentSurfaceCreated(jobject surface, gl::SurfaceTexture* texture);
  void ContentOverlaySurfaceCreated(jobject surface,
                                    gl::SurfaceTexture* texture);
  void GvrDelegateReady(gvr::ViewerType viewer_type,
                        device::mojom::VRDisplayFrameTransportOptionsPtr);

  device::mojom::VRDisplayFrameTransportOptionsPtr
  GetVRDisplayFrameTransportOptions();
  void DialogSurfaceCreated(jobject surface, gl::SurfaceTexture* texture);

  void BufferBoundsChanged(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& object,
                           jint content_width,
                           jint content_height,
                           jint overlay_width,
                           jint overlay_height);

  void SetHighAccuracyLocation(bool high_accuracy_location);

  void ForceExitVr();
  void ExitPresent();
  void ExitFullscreen();
  void LogUnsupportedModeUserMetric(UiUnsupportedMode mode);
  void OnUnsupportedMode(UiUnsupportedMode mode);
  void OnExitVrPromptResult(UiUnsupportedMode reason,
                            ExitVrPromptChoice choice);
  void OnContentScreenBoundsChanged(const gfx::SizeF& bounds);
  void SetVoiceSearchActive(bool active);
  void StartAutocomplete(const AutocompleteRequest& request);
  void StopAutocomplete();
  bool HasAudioPermission();

  void ClearFocusedElement();
  void ProcessContentGesture(std::unique_ptr<blink::WebInputEvent> event,
                             int content_id);

  void ProcessDialogGesture(std::unique_ptr<blink::WebInputEvent> event);

  void SetAlertDialog(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      int width,
                      int height);
  void CloseAlertDialog(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void SetAlertDialogSize(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          int width,
                          int height);

  void ConnectPresentingService(
      device::mojom::VRSubmitFrameClientPtr submit_client,
      device::mojom::VRPresentationProviderRequest request,
      device::mojom::VRDisplayInfoPtr display_info,
      device::mojom::VRRequestPresentOptionsPtr present_options);

  // device::GvrGamepadDataProvider implementation.
  void UpdateGamepadData(device::GvrGamepadData) override;
  void RegisterGvrGamepadDataFetcher(device::GvrGamepadDataFetcher*) override;

  // device::CardboardGamepadDataProvider implementation.
  void RegisterCardboardGamepadDataFetcher(
      device::CardboardGamepadDataFetcher*) override;

  // ChromeToolbarModelDelegate implementation.
  content::WebContents* GetActiveWebContents() const override;
  bool ShouldDisplayURL() const override;

  void OnVoiceResults(const base::string16& result) override;

  void OnAssetsLoaded(AssetsLoadStatus status,
                      std::unique_ptr<Assets> assets,
                      const base::Version& component_version);

  void AcceptDoffPromptForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  ~VrShell() override;
  void PostToGlThread(const base::Location& from_here, base::OnceClosure task);
  void SetUiState();

  void ProcessTabArray(JNIEnv* env, jobjectArray tabs, bool incognito);

  void PollMediaAccessFlag();

  bool HasDaydreamSupport(JNIEnv* env);

  void ExitVrDueToUnsupportedMode(UiUnsupportedMode mode);

  content::WebContents* GetNonNativePageWebContents() const;

  void LoadAssets();
  void OnAssetsComponentReady();
  void OnAssetsComponentWaitTimeout();

  bool vr_shell_enabled_;

  bool webvr_mode_ = false;
  bool web_vr_autopresentation_expected_ = false;

  content::WebContents* web_contents_ = nullptr;
  bool web_contents_is_native_page_ = false;
  base::android::ScopedJavaGlobalRef<jobject> j_motion_event_synthesizer_;

  std::unique_ptr<VrWebContentsObserver> vr_web_contents_observer_;
  std::unique_ptr<VrInputConnection> vr_input_connection_;

  VrShellDelegate* delegate_provider_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_;

  std::unique_ptr<AndroidUiGestureTarget> android_ui_gesture_target_;
  std::unique_ptr<AndroidUiGestureTarget> dialog_gesture_target_;
  std::unique_ptr<VrMetricsHelper> metrics_helper_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<VrGLThread> gl_thread_;
  BrowserUiInterface* ui_;

  // These instances make use of ui_ (provided by gl_thread_), and hence must be
  // destroyed before gl_thread_;
  std::unique_ptr<ToolbarHelper> toolbar_;
  std::unique_ptr<vr::AutocompleteController> autocomplete_controller_;
  std::unique_ptr<SpeechRecognizer> speech_recognizer_;

  bool reprojected_rendering_;

  device::mojom::GeolocationConfigPtr geolocation_config_;

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

  // For GetVRDisplayFrameTransportOptions()
  device::mojom::VRDisplayFrameTransportOptionsPtr frame_transport_options_;

  // Content id
  int content_id_ = 0;

  gfx::SizeF display_size_meters_;
  gfx::Size display_size_pixels_;

  gl::SurfaceTexture* content_surface_texture_ = nullptr;
  gl::SurfaceTexture* overlay_surface_texture_ = nullptr;
  gl::SurfaceTexture* ui_surface_texture_ = nullptr;

  base::Timer waiting_for_assets_component_timer_;

  base::WeakPtrFactory<VrShell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShell);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_SHELL_H_

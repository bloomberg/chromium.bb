// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_GL_THREAD_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_GL_THREAD_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/browser/android/vr_shell/gl_browser_interface.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_interface.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace vr {
class UiScene;
class UiSceneManager;
}  // namespace vr

namespace vr_shell {

class VrShell;
class VrShellGl;

class VrGLThread : public base::Thread,
                   public GlBrowserInterface,
                   public vr::UiBrowserInterface,
                   public vr::UiInterface {
 public:
  VrGLThread(
      const base::WeakPtr<VrShell>& weak_vr_shell,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gvr_context* gvr_api,
      bool initially_web_vr,
      bool web_vr_autopresentation_expected,
      bool in_cct,
      bool reprojected_rendering,
      bool daydream_support);

  ~VrGLThread() override;
  base::WeakPtr<VrShellGl> GetVrShellGl() { return weak_vr_shell_gl_; }

  // GlBrowserInterface implementation (VrShellGl calling out to UI/VrShell).
  void ContentSurfaceChanged(jobject surface) override;
  void GvrDelegateReady(gvr::ViewerType viewer_type) override;
  void UpdateGamepadData(device::GvrGamepadData) override;
  void AppButtonClicked() override;
  void AppButtonGesturePerformed(vr::UiInterface::Direction direction) override;
  void ProcessContentGesture(
      std::unique_ptr<blink::WebInputEvent> event) override;
  void ForceExitVr() override;
  void RunVRDisplayInfoCallback(
      const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
      device::mojom::VRDisplayInfoPtr* info) override;
  void OnContentPaused(bool enabled) override;
  void ToggleCardboardGamepad(bool enabled) override;
  void OnGlInitialized(unsigned int content_texture_id) override;
  void OnWebVrFrameAvailable() override;

  // vr::UiBrowserInterface implementation (UI calling to VrShell).
  void ExitPresent() override;
  void ExitFullscreen() override;
  void NavigateBack() override;
  void ExitCct() override;
  void OnUnsupportedMode(vr::UiUnsupportedMode mode) override;
  void OnExitVrPromptResult(vr::UiUnsupportedMode reason,
                            vr::ExitVrPromptChoice choice) override;

  // vr::UiInterface implementation (VrShell and GL calling to the UI).
  void SetFullscreen(bool enabled) override;
  void SetIncognito(bool incognito) override;
  void SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward) override;
  void SetLoadProgress(float progress) override;
  void SetLoading(bool loading) override;
  void SetToolbarState(const vr::ToolbarState& state) override;
  void SetWebVrMode(bool enabled, bool show_toast) override;
  void SetWebVrSecureOrigin(bool secure) override;
  void SetVideoCapturingIndicator(bool enabled) override;
  void SetScreenCapturingIndicator(bool enabled) override;
  void SetAudioCapturingIndicator(bool enabled) override;
  void SetBluetoothConnectedIndicator(bool enabled) override;
  void SetLocationAccessIndicator(bool enabled) override;
  void SetIsExiting() override;
  void SetSplashScreenIcon(const SkBitmap& bitmap) override;
  void SetExitVrPromptEnabled(bool enabled,
                              vr::UiUnsupportedMode reason) override;

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  // Created on GL thread.
  std::unique_ptr<vr::UiScene> scene_;
  std::unique_ptr<vr::UiSceneManager> scene_manager_;
  base::WeakPtr<vr::UiSceneManager> weak_scene_manager_;
  std::unique_ptr<VrShellGl> vr_shell_gl_;
  base::WeakPtr<VrShellGl> weak_vr_shell_gl_;

  // This state is used for initializing vr_shell_gl_.
  base::WeakPtr<VrShell> weak_vr_shell_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  gvr_context* gvr_api_;
  bool initially_web_vr_;
  bool web_vr_autopresentation_expected_;
  bool in_cct_;
  bool reprojected_rendering_;
  bool daydream_support_;

  DISALLOW_COPY_AND_ASSIGN(VrGLThread);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_GL_THREAD_H_

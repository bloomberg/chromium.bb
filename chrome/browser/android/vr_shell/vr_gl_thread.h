// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_GL_THREAD_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_GL_THREAD_H_

#include <memory>

#include "base/android/java_handler_thread.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr_shell/gl_browser_interface.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_interface.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace vr_shell {

class VrShell;
class VrShellGl;

class VrGLThread : public base::android::JavaHandlerThread,
                   public GlBrowserInterface,
                   public vr::UiBrowserInterface,
                   public vr::BrowserUiInterface {
 public:
  VrGLThread(
      const base::WeakPtr<VrShell>& weak_vr_shell,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gvr_context* gvr_api,
      const vr::UiInitialState& ui_initial_state,
      bool reprojected_rendering,
      bool daydream_support);

  ~VrGLThread() override;
  base::WeakPtr<VrShellGl> GetVrShellGl();

  // vr::GlBrowserInterface implementation (GL calling to VrShell).
  void ContentSurfaceChanged(jobject surface) override;
  void GvrDelegateReady(gvr::ViewerType viewer_type) override;
  void UpdateGamepadData(device::GvrGamepadData) override;
  void ProcessContentGesture(std::unique_ptr<blink::WebInputEvent> event,
                             int content_id) override;
  void ForceExitVr() override;
  void OnContentPaused(bool enabled) override;
  void ToggleCardboardGamepad(bool enabled) override;

  // vr::UiBrowserInterface implementation (UI calling to VrShell).
  void ExitPresent() override;
  void ExitFullscreen() override;
  void NavigateBack() override;
  void ExitCct() override;
  void OnUnsupportedMode(vr::UiUnsupportedMode mode) override;
  void OnExitVrPromptResult(vr::UiUnsupportedMode reason,
                            vr::ExitVrPromptChoice choice) override;
  void OnContentScreenBoundsChanged(const gfx::SizeF& bounds) override;
  void SetVoiceSearchActive(bool active) override;

  // vr::BrowserUiInterface implementation (Browser calling to UI).
  void SetWebVrMode(bool enabled, bool show_toast) override;
  void SetFullscreen(bool enabled) override;
  void SetToolbarState(const vr::ToolbarState& state) override;
  void SetIncognito(bool incognito) override;
  void SetLoading(bool loading) override;
  void SetLoadProgress(float progress) override;
  void SetIsExiting() override;
  void SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward) override;
  void SetVideoCapturingIndicator(bool enabled) override;
  void SetScreenCapturingIndicator(bool enabled) override;
  void SetAudioCapturingIndicator(bool enabled) override;
  void SetBluetoothConnectedIndicator(bool enabled) override;
  void SetLocationAccessIndicator(bool enabled) override;
  void SetExitVrPromptEnabled(bool enabled,
                              vr::UiUnsupportedMode reason) override;

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  bool OnMainThread() const;
  bool OnGlThread() const;

  // Created on GL thread.
  std::unique_ptr<VrShellGl> vr_shell_gl_;

  base::WeakPtr<VrShell> weak_vr_shell_;
  base::WeakPtr<vr::BrowserUiInterface> browser_ui_;

  // This state is used for initializing vr_shell_gl_.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  gvr_context* gvr_api_;
  vr::UiInitialState ui_initial_state_;
  bool reprojected_rendering_;
  bool daydream_support_;

  DISALLOW_COPY_AND_ASSIGN(VrGLThread);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_GL_THREAD_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_H_
#define CHROME_BROWSER_VR_UI_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/ui_interface.h"

namespace vr {
class BrowserUiInterface;
class ContentInputDelegate;
class UiBrowserInterface;
class UiInputManager;
class UiRenderer;
class UiScene;
class UiSceneManager;
class VrShellRenderer;
struct Model;
}  // namespace vr

namespace vr {

struct UiInitialState {
  bool in_cct = false;
  bool in_web_vr = false;
  bool web_vr_autopresentation_expected = false;
};

// This class manages all GLThread owned objects and GL rendering for VrShell.
// It is not threadsafe and must only be used on the GL thread.
class Ui : public BrowserUiInterface, public UiInterface {
 public:
  Ui(UiBrowserInterface* browser,
     ContentInputDelegate* content_input_delegate,
     const vr::UiInitialState& ui_initial_state);
  ~Ui() override;

  // TODO(crbug.com/767957): Refactor to hide these behind the UI interface.
  UiScene* scene() { return scene_.get(); }
  VrShellRenderer* vr_shell_renderer() { return vr_shell_renderer_.get(); }
  UiRenderer* ui_renderer() { return ui_renderer_.get(); }
  UiInputManager* input_manager() { return input_manager_.get(); }

  base::WeakPtr<vr::BrowserUiInterface> GetBrowserUiWeakPtr();

  // BrowserUiInterface
  void SetWebVrMode(bool enabled, bool show_toast) override;
  void SetFullscreen(bool enabled) override;
  void SetToolbarState(const ToolbarState& state) override;
  void SetIncognito(bool enabled) override;
  void SetLoading(bool loading) override;
  void SetLoadProgress(float progress) override;
  void SetIsExiting() override;
  void SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward) override;
  void SetVideoCapturingIndicator(bool enabled) override;
  void SetScreenCapturingIndicator(bool enabled) override;
  void SetAudioCapturingIndicator(bool enabled) override;
  void SetBluetoothConnectedIndicator(bool enabled) override;
  void SetLocationAccessIndicator(bool enabled) override;
  void SetExitVrPromptEnabled(bool enabled, UiUnsupportedMode reason) override;

  // UiInterface
  bool ShouldRenderWebVr() override;
  void OnGlInitialized(
      unsigned int content_texture_id,
      UiElementRenderer::TextureLocation content_location) override;
  void OnAppButtonClicked() override;
  void OnAppButtonGesturePerformed(UiInterface::Direction direction) override;
  void OnProjMatrixChanged(const gfx::Transform& proj_matrix) override;
  void OnWebVrFrameAvailable() override;
  void OnWebVrTimedOut() override;

 private:
  // This state may be further abstracted into a SkiaUi object.
  std::unique_ptr<vr::UiScene> scene_;
  std::unique_ptr<vr::Model> model_;
  std::unique_ptr<vr::UiSceneManager> scene_manager_;
  std::unique_ptr<vr::VrShellRenderer> vr_shell_renderer_;
  std::unique_ptr<vr::UiInputManager> input_manager_;
  std::unique_ptr<vr::UiRenderer> ui_renderer_;

  base::WeakPtrFactory<Ui> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Ui);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_H_

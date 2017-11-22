// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_H_
#define CHROME_BROWSER_VR_UI_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/platform_controller.h"
#include "chrome/browser/vr/ui_element_renderer.h"

namespace vr {
class BrowserUiInterface;
class ContentInputDelegate;
class ContentInputForwarder;
class SkiaSurfaceProvider;
class UiBrowserInterface;
class UiInputManager;
class UiRenderer;
class UiScene;
struct ControllerModel;
struct Model;
struct OmniboxSuggestions;
struct ReticleModel;

struct UiInitialState {
  bool in_cct = false;
  bool in_web_vr = false;
  bool web_vr_autopresentation_expected = false;
  bool browsing_disabled = false;
  bool has_or_can_request_audio_permission = true;
};

// This class manages all GLThread owned objects and GL rendering for VrShell.
// It is not threadsafe and must only be used on the GL thread.
class Ui : public BrowserUiInterface {
 public:
  Ui(UiBrowserInterface* browser,
     ContentInputForwarder* content_input_forwarder,
     const UiInitialState& ui_initial_state);

  Ui(UiBrowserInterface* browser,
     std::unique_ptr<ContentInputDelegate> content_input_delegate,
     const UiInitialState& ui_initial_state);

  ~Ui() override;

  // TODO(crbug.com/767957): Refactor to hide these behind the UI interface.
  UiScene* scene() { return scene_.get(); }
  UiElementRenderer* ui_element_renderer() {
    return ui_element_renderer_.get();
  }
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
  void SetVideoCaptureEnabled(bool enabled) override;
  void SetScreenCaptureEnabled(bool enabled) override;
  void SetAudioCaptureEnabled(bool enabled) override;
  void SetBluetoothConnected(bool enabled) override;
  void SetLocationAccess(bool enabled) override;
  void SetExitVrPromptEnabled(bool enabled, UiUnsupportedMode reason) override;
  void SetSpeechRecognitionEnabled(bool enabled) override;
  void SetRecognitionResult(const base::string16& result) override;
  void OnSpeechRecognitionStateChanged(int new_state) override;
  void SetOmniboxSuggestions(
      std::unique_ptr<OmniboxSuggestions> suggestions) override;

  bool ShouldRenderWebVr();
  void OnGlInitialized(unsigned int content_texture_id,
                       UiElementRenderer::TextureLocation content_location,
                       bool use_ganesh);
  void OnAppButtonClicked();
  void OnAppButtonGesturePerformed(
      PlatformController::SwipeDirection direction);
  void OnControllerUpdated(const ControllerModel& controller_model,
                           const ReticleModel& reticle_model);
  void OnProjMatrixChanged(const gfx::Transform& proj_matrix);
  void OnWebVrFrameAvailable();
  void OnWebVrTimedOut();
  void OnWebVrTimeoutImminent();
  bool IsControllerVisible() const;
  void OnSwapContents(int new_content_id);
  void OnContentBoundsChanged(int width, int height);
  void OnPlatformControllerInitialized(PlatformController* controller);

  Model* model_for_test() { return model_.get(); }

  void Dump();

 private:
  UiBrowserInterface* browser_;

  // This state may be further abstracted into a SkiaUi object.
  std::unique_ptr<vr::UiScene> scene_;
  std::unique_ptr<vr::Model> model_;
  std::unique_ptr<vr::ContentInputDelegate> content_input_delegate_;
  std::unique_ptr<vr::UiElementRenderer> ui_element_renderer_;
  std::unique_ptr<vr::UiInputManager> input_manager_;
  std::unique_ptr<vr::UiRenderer> ui_renderer_;
  std::unique_ptr<SkiaSurfaceProvider> provider_;

  base::WeakPtrFactory<Ui> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Ui);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_H_

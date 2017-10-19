// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_MANAGER_H_
#define CHROME_BROWSER_VR_UI_SCENE_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/elements/simple_textured_element.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr {

class ContentElement;
class ContentInputDelegate;
class Grid;
class Rect;
class TransientElement;
class WebVrUrlToast;
class UiBrowserInterface;
class UiElement;
class UiScene;
class UrlBar;
class ExitPrompt;
struct Model;
struct UiInitialState;

// The scene manager creates and maintains a UiElement hierarchy.
//
// kRoot
//   k2dBrowsingRoot
//     k2dBrowsingBackground
//       kBackgroundLeft
//       kBackgroundRight
//       kBackgroundTop
//       kBackgroundBottom
//       kBackgroundFront
//       kBackgroundBack
//       kFloor
//       kCeiling
//     k2dBrowsingForeground
//       k2dBrowsingContentGroup
//         kBackplane
//         kContentQuad
//         kIndicatorLayout
//           kAudioCaptureIndicator
//           kVideoCaptureIndicator
//           kScreenCaptureIndicator
//           kLocationAccessIndicator
//           kBluetoothConnectedIndicator
//       kExitPrompt
//         kExitPromptBackplane
//       kExclusiveScreenToastTransientParent
//         kExclusiveScreenToast
//       kCloseButton
//       kUrlBar
//         kLoadingIndicator
//         kExitButton
//     kFullscreenToast
//     kScreenDimmer
//     k2dBrowsingViewportAwareRoot
//       kExitWarning
//   kWebVrRoot
//     kWebVrViewportAwareRoot
//       kExclusiveScreenToastTransientParent
//         kExclusiveScreenToastViewportAware
//       kWebVrPermanentHttpSecurityWarning
//       kWebVrTransientHttpSecurityWarningTransientParent
//         kWebVrTransientHttpSecurityWarning
//       kWebVrUrlToastTransientParent
//         kWebVrUrlToast
//   kSplashScreenRoot
//     kSplashScreenViewportAwareRoot
//       kSplashScreenTransientParent
//         kSplashScreenText
//           kSplashScreenBackground
//
// TODO(vollick): The above hierarchy is complex, brittle, and would be easier
// to manage if it were specified in a declarative format.
class UiSceneManager {
 public:
  UiSceneManager(UiBrowserInterface* browser,
                 UiScene* scene,
                 ContentInputDelegate* content_input_delegate,
                 Model* model,
                 const UiInitialState& ui_initial_state);
  ~UiSceneManager();

  // BrowserUiInterface support methods.
  void SetFullscreen(bool fullscreen);
  void SetIncognito(bool incognito);
  void SetToolbarState(const ToolbarState& state);
  void SetWebVrMode(bool web_vr, bool show_toast);
  void SetIsExiting();
  void SetVideoCapturingIndicator(bool enabled);
  void SetScreenCapturingIndicator(bool enabled);
  void SetAudioCapturingIndicator(bool enabled);
  void SetLocationAccessIndicator(bool enabled);
  void SetBluetoothConnectedIndicator(bool enabled);
  void SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward);
  void SetExitVrPromptEnabled(bool enabled, UiUnsupportedMode reason);

  // UiInterface support methods.
  bool ShouldRenderWebVr();
  void OnGlInitialized(unsigned int content_texture_id,
                       UiElementRenderer::TextureLocation content_location);
  void OnAppButtonClicked();
  void OnAppButtonGesturePerformed(UiInterface::Direction direction);
  void OnProjMatrixChanged(const gfx::Transform& proj_matrix);
  void OnWebVrFrameAvailable();
  void OnWebVrTimedOut();

  void OnSplashScreenHidden(TransientElementHideReason);
  void OnSecurityIconClickedForTesting();
  void OnExitPromptChoiceForTesting(bool chose_exit);

  // TODO(vollick): this should move to the model.
  const ColorScheme& color_scheme() const;

 private:
  void Create2dBrowsingSubtreeRoots();
  void CreateWebVrRoot();
  void CreateScreenDimmer();
  void CreateWebVRExitWarning();
  void CreateSystemIndicators();
  void CreateContentQuad(ContentInputDelegate* delegate);
  void CreateSplashScreen();
  void CreateUnderDevelopmentNotice();
  void CreateBackground();
  void CreateViewportAwareRoot();
  void CreateUrlBar(Model* model);
  void CreateWebVrUrlToast();
  void CreateCloseButton();
  void CreateExitPrompt();
  void CreateToasts();

  void ConfigureScene();
  void ConfigureExclusiveScreenToast();
  void ConfigureIndicators();
  void ConfigureBackgroundColor();
  void OnBackButtonClicked();
  void OnSecurityIconClicked();
  void OnExitPromptChoice(bool chose_exit);
  void OnExitPromptBackplaneClicked();
  void OnCloseButtonClicked();
  void OnUnsupportedMode(UiUnsupportedMode mode);
  ColorScheme::Mode mode() const;

  TransientElement* AddTransientParent(UiElementName name,
                                       UiElementName parent_name,
                                       int timeout_seconds,
                                       bool animate_opacity);

  UiBrowserInterface* browser_;
  UiScene* scene_;

  // UI element pointers (not owned by the scene manager).
  TransientElement* exclusive_screen_toast_transient_parent_ = nullptr;
  TransientElement* exclusive_screen_toast_viewport_aware_transient_parent_ =
      nullptr;
  ShowUntilSignalTransientElement* splash_screen_transient_parent_ = nullptr;
  ExitPrompt* exit_prompt_ = nullptr;
  UiElement* exit_prompt_backplane_ = nullptr;
  UiElement* exit_warning_ = nullptr;
  ContentElement* main_content_ = nullptr;
  UiElement* audio_capture_indicator_ = nullptr;
  UiElement* bluetooth_connected_indicator_ = nullptr;
  UiElement* video_capture_indicator_ = nullptr;
  UiElement* screen_capture_indicator_ = nullptr;
  UiElement* location_access_indicator_ = nullptr;
  UiElement* screen_dimmer_ = nullptr;
  Rect* ceiling_ = nullptr;
  Grid* floor_ = nullptr;
  UiElement* close_button_ = nullptr;
  UrlBar* url_bar_ = nullptr;
  TransientElement* webvr_url_toast_transient_parent_ = nullptr;
  WebVrUrlToast* webvr_url_toast_ = nullptr;

  std::vector<UiElement*> system_indicators_;

  bool in_cct_;
  bool web_vr_mode_;
  bool web_vr_show_toast_ = false;
  bool started_for_autopresentation_ = false;
  // Flag to indicate that we're waiting for the first WebVR frame to show up
  // before we hide the splash screen. This is used in the case of WebVR
  // auto-presentation.
  bool showing_web_vr_splash_screen_ = false;
  bool prompting_to_exit_ = false;
  bool exiting_ = false;
  bool browsing_disabled_ = false;

  bool fullscreen_ = false;
  bool incognito_ = false;
  bool audio_capturing_ = false;
  bool video_capturing_ = false;
  bool screen_capturing_ = false;
  bool location_access_ = false;
  bool bluetooth_connected_ = false;
  UiUnsupportedMode exit_vr_prompt_reason_ = UiUnsupportedMode::kCount;

  std::vector<Rect*> background_panels_;
  std::vector<UiElement*> content_elements_;
  std::vector<UiElement*> control_elements_;

  gfx::SizeF last_content_screen_bounds_;
  float last_content_aspect_ratio_ = 0.0f;

  DISALLOW_COPY_AND_ASSIGN(UiSceneManager);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_MANAGER_H_

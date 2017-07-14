// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_MANAGER_H_
#define CHROME_BROWSER_VR_UI_SCENE_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/elements/simple_textured_element.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr {

class ExclusiveScreenToast;
class LoadingIndicator;
class SplashScreenIcon;
class TransientUrlBar;
class UiBrowserInterface;
class UiElement;
class UiScene;
class UrlBar;

class UiSceneManager {
 public:
  UiSceneManager(UiBrowserInterface* browser,
                 UiScene* scene,
                 bool in_cct,
                 bool in_web_vr,
                 bool web_vr_autopresentation_expected);
  ~UiSceneManager();

  base::WeakPtr<UiSceneManager> GetWeakPtr();

  void SetFullscreen(bool fullscreen);
  void SetIncognito(bool incognito);
  void SetToolbarState(const ToolbarState& state);
  void SetWebVrSecureOrigin(bool secure);
  void SetWebVrMode(bool web_vr, bool show_toast);
  void SetLoading(bool loading);
  void SetLoadProgress(float progress);
  void SetIsExiting();
  void SetVideoCapturingIndicator(bool enabled);
  void SetScreenCapturingIndicator(bool enabled);
  void SetAudioCapturingIndicator(bool enabled);
  void SetLocationAccessIndicator(bool enabled);
  void SetSplashScreenIcon(const SkBitmap& bitmap);
  void SetBluetoothConnectedIndicator(bool enabled);

  // These methods are currently stubbed.
  void SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward);

  void OnGLInitialized();
  void OnAppButtonClicked();
  void OnAppButtonGesturePerformed(UiInterface::Direction direction);
  void OnWebVrFrameAvailable();

  void OnSecurityIconClickedForTesting();
  void OnExitPromptPrimaryButtonClickedForTesting();

 private:
  void CreateScreenDimmer();
  void CreateSecurityWarnings();
  void CreateSystemIndicators();
  void CreateContentQuad();
  void CreateSplashScreen();
  void CreateBackground();
  void CreateUrlBar();
  void CreateTransientUrlBar();
  void CreateCloseButton();
  void CreateExitPrompt();
  void CreateToasts();

  void ConfigureScene();
  void ConfigureSecurityWarnings();
  void ConfigureExclusiveScreenToast();
  void ConfigureIndicators();
  void UpdateBackgroundColor();
  void CloseExitPrompt();
  void OnBackButtonClicked();
  void OnSecurityIconClicked();
  void OnExitPromptPrimaryButtonClicked();
  void OnExitPromptSecondaryButtonClicked();
  void OnExitPromptBackplaneClicked();
  void OnCloseButtonClicked();
  void OnUnsupportedMode(UiUnsupportedMode mode);
  int AllocateId();
  ColorScheme::Mode mode() const;
  const ColorScheme& color_scheme() const;

  UiBrowserInterface* browser_;
  UiScene* scene_;

  // UI element pointers (not owned by the scene manager).
  UiElement* permanent_security_warning_ = nullptr;
  TransientSecurityWarning* transient_security_warning_ = nullptr;
  ExclusiveScreenToast* exclusive_screen_toast_ = nullptr;
  UiElement* exit_prompt_ = nullptr;
  UiElement* exit_prompt_backplane_ = nullptr;
  UiElement* exit_warning_ = nullptr;
  UiElement* main_content_ = nullptr;
  UiElement* audio_capture_indicator_ = nullptr;
  UiElement* bluetooth_connected_indicator_ = nullptr;
  UiElement* video_capture_indicator_ = nullptr;
  UiElement* screen_capture_indicator_ = nullptr;
  UiElement* location_access_indicator_ = nullptr;
  UiElement* screen_dimmer_ = nullptr;
  UiElement* ceiling_ = nullptr;
  UiElement* floor_ = nullptr;
  UiElement* close_button_ = nullptr;
  SplashScreenIcon* splash_screen_icon_ = nullptr;
  UrlBar* url_bar_ = nullptr;
  TransientUrlBar* transient_url_bar_ = nullptr;
  LoadingIndicator* loading_indicator_ = nullptr;

  std::vector<UiElement*> system_indicators_;

  bool in_cct_;
  bool web_vr_mode_;
  bool web_vr_show_toast_ = false;
  bool started_for_autopresentation_ = false;
  // Flag to indicate that we're waiting for the first WebVR frame to show up
  // before we hide the splash screen. This is used in the case of WebVR
  // auto-presentation.
  bool waiting_for_first_web_vr_frame_ = false;
  bool secure_origin_ = false;
  bool fullscreen_ = false;
  bool incognito_ = false;
  bool audio_capturing_ = false;
  bool video_capturing_ = false;
  bool screen_capturing_ = false;
  bool location_access_ = false;
  bool bluetooth_connected_ = false;

  int next_available_id_ = 1;

  std::vector<UiElement*> content_elements_;
  std::vector<UiElement*> background_elements_;
  std::vector<UiElement*> control_elements_;

  base::WeakPtrFactory<UiSceneManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiSceneManager);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_MANAGER_H_

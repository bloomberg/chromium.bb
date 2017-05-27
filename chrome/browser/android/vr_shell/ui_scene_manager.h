// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_MANAGER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/android/vr_shell/ui_unsupported_mode.h"
#include "device/vr/vr_types.h"

namespace vr_shell {

class LoadingIndicator;
class UiElement;
class UiScene;
class UrlBar;
class VrBrowserInterface;

struct ColorScheme;

class UiSceneManager {
 public:
  UiSceneManager(VrBrowserInterface* browser,
                 UiScene* scene,
                 bool in_cct,
                 bool in_web_vr);
  ~UiSceneManager();

  base::WeakPtr<UiSceneManager> GetWeakPtr();

  void SetFullscreen(bool fullscreen);
  void SetURL(const GURL& gurl);
  void SetWebVrSecureOrigin(bool secure);
  void SetWebVrMode(bool web_vr);
  void SetSecurityLevel(security_state::SecurityLevel level);
  void SetLoading(bool loading);
  void SetLoadProgress(float progress);
  void SetIsExiting();
  void SetVideoCapturingIndicator(bool enabled);
  void SetScreenCapturingIndicator(bool enabled);
  void SetAudioCapturingIndicator(bool enabled);

  // These methods are currently stubbed.
  void SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward);

  void OnAppButtonClicked();
  void OnAppButtonGesturePerformed(UiInterface::Direction direction);

 private:
  void CreateScreenDimmer();
  void CreateSecurityWarnings();
  void CreateSystemIndicators();
  void CreateContentQuad();
  void CreateBackground();
  void CreateUrlBar();
  void CreateCloseButton();
  void CreateExitWarning();

  void ConfigureScene();
  void ConfigureSecurityWarnings();
  void UpdateBackgroundColor();
  void OnSecurityWarningTimer();
  void OnBackButtonClicked();
  void OnCloseButtonClicked();
  void OnUnsupportedMode(UiUnsupportedMode mode);
  int AllocateId();
  const ColorScheme& color_scheme() const;

  VrBrowserInterface* browser_;
  UiScene* scene_;

  // UI element pointers (not owned by the scene manager).
  UiElement* permanent_security_warning_ = nullptr;
  UiElement* transient_security_warning_ = nullptr;
  UiElement* exit_warning_ = nullptr;
  UiElement* main_content_ = nullptr;
  UiElement* audio_capture_indicator_ = nullptr;
  UiElement* video_capture_indicator_ = nullptr;
  UiElement* screen_capture_indicator_ = nullptr;
  UiElement* screen_dimmer_ = nullptr;
  UiElement* ceiling_ = nullptr;
  UiElement* floor_ = nullptr;
  UiElement* floor_grid_ = nullptr;
  UrlBar* url_bar_ = nullptr;
  LoadingIndicator* loading_indicator_ = nullptr;

  bool in_cct_;
  bool web_vr_mode_;
  bool secure_origin_ = false;
  bool fullscreen_ = false;
  bool is_exiting_ = false;
  bool audio_capturing_ = false;
  bool video_capturing_ = false;
  bool screen_capturing_ = false;

  int next_available_id_ = 1;

  std::vector<UiElement*> content_elements_;
  std::vector<UiElement*> control_elements_;

  base::OneShotTimer security_warning_timer_;

  base::WeakPtrFactory<UiSceneManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiSceneManager);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_MANAGER_H_

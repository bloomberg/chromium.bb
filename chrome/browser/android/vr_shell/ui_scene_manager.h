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

class GURL;

namespace vr_shell {

class UiElement;
class UiScene;
class UrlBar;
class VrBrowserInterface;

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
  // These methods are currently stubbed.
  void SetSecurityLevel(int level);
  void SetLoading(bool loading);
  void SetLoadProgress(double progress);
  void SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward);

  void OnAppButtonClicked();
  void OnAppButtonGesturePerformed(UiInterface::Direction direction);

 private:
  void CreateSecurityWarnings();
  void CreateContentQuad();
  void CreateBackground();
  void CreateUrlBar();

  void ConfigureSecurityWarnings();
  void OnSecurityWarningTimer();
  int AllocateId();

  VrBrowserInterface* browser_;
  UiScene* scene_;

  // UI element pointers (not owned by the scene manager).
  UiElement* permanent_security_warning_ = nullptr;
  UiElement* transient_security_warning_ = nullptr;
  UiElement* main_content_ = nullptr;
  UrlBar* url_bar_ = nullptr;

  bool in_cct_;
  bool web_vr_mode_;
  bool secure_origin_ = false;
  bool content_rendering_enabled_ = true;

  int next_available_id_ = 1;

  std::vector<UiElement*> browser_ui_elements_;

  base::OneShotTimer security_warning_timer_;

  base::WeakPtrFactory<UiSceneManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiSceneManager);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_MANAGER_H_

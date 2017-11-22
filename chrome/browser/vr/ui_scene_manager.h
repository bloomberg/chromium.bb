// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_MANAGER_H_
#define CHROME_BROWSER_VR_UI_SCENE_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element_name.h"

namespace vr {

class ContentInputDelegate;
class UiBrowserInterface;
class UiScene;
struct Model;

// The scene manager creates our scene hierarchy.
class UiSceneManager {
 public:
  UiSceneManager(UiBrowserInterface* browser,
                 UiScene* scene,
                 ContentInputDelegate* content_input_delegate,
                 Model* model);
  ~UiSceneManager();

  void CreateScene();

 private:
  void Create2dBrowsingSubtreeRoots();
  void CreateWebVrRoot();
  void CreateWebVRExitWarning();
  void CreateSystemIndicators();
  void CreateContentQuad();
  void CreateSplashScreen();
  void CreateUnderDevelopmentNotice();
  void CreateBackground();
  void CreateViewportAwareRoot();
  void CreateUrlBar();
  void CreateSuggestionList();
  void CreateWebVrUrlToast();
  void CreateCloseButton();
  void CreateExitPrompt();
  void CreateAudioPermissionPrompt();
  void CreateToasts();
  void CreateVoiceSearchUiGroup();
  void CreateController();

  UiBrowserInterface* browser_;
  UiScene* scene_;
  ContentInputDelegate* content_input_delegate_;
  Model* model_;

  DISALLOW_COPY_AND_ASSIGN(UiSceneManager);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_MANAGER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_MANAGER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"

namespace vr_shell {

struct UiElement;
class UiScene;

class UiSceneManager {
 public:
  explicit UiSceneManager(UiScene* scene);
  ~UiSceneManager();

  base::WeakPtr<UiSceneManager> GetWeakPtr();

  void UpdateScene(std::unique_ptr<base::ListValue> commands);

  void SetWebVRSecureOrigin(bool secure);
  void SetWebVRMode(bool web_vr);

 private:
  void ConfigureSecurityWarnings();
  void OnSecurityWarningTimer();

  UiScene* scene_;

  // UI elemenet pointers (not owned by the scene manager).
  UiElement* permanent_security_warning_ = nullptr;
  UiElement* transient_security_warning_ = nullptr;

  bool web_vr_mode_ = false;
  bool secure_origin_ = false;

  base::OneShotTimer security_warning_timer_;

  base::WeakPtrFactory<UiSceneManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiSceneManager);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_MANAGER_H_

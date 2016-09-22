// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_H_

#include <memory>
#include <vector>

#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/ui_elements.h"

namespace vr_shell {

class UiScene {
 public:
  UiScene();
  virtual ~UiScene();

  void AddUiElement(std::unique_ptr<ContentRectangle>& element);
  void RemoveUiElement(int id);

  // Add an animation to the scene, on element |id|.
  void AddAnimation(int id, std::unique_ptr<Animation>& animation);

  // Remove an animation from element |id|, of type |property|.
  void RemoveAnimation(int id, Animation::Property property);

  // Update the positions of all elements in the scene, according to active
  // animations, desired screen tilt and time.  The units of time are
  // arbitrary, but must match the unit used in animations.
  void UpdateTransforms(float screen_tilt, int64_t time);

  const std::vector<std::unique_ptr<ContentRectangle>>& GetUiElements() const;

  ContentRectangle* GetElementById(int id);

 private:
  void ApplyRecursiveTransforms(const ContentRectangle& element,
                                ReversibleTransform* transform);
  void ApplyAnchoring(const ContentRectangle& parent, XAnchoring x_anchoring,
                      YAnchoring y_anchoring, ReversibleTransform* transform);
  void RemoveAnimationType(ContentRectangle *element,
                           Animation::Property property);
  std::vector<std::unique_ptr<ContentRectangle>> ui_elements_;

  DISALLOW_COPY_AND_ASSIGN(UiScene);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/vr_math.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace vr_shell {

class Animation;
struct ContentRectangle;
struct ReversibleTransform;

class UiScene {
 public:
  enum Command {
    ADD_ELEMENT,
    UPDATE_ELEMENT,
    REMOVE_ELEMENT,
    ADD_ANIMATION,
    REMOVE_ANIMATION,
    UPDATE_BACKGROUND,
  };

  UiScene();
  virtual ~UiScene();

  void AddUiElement(std::unique_ptr<ContentRectangle>& element);

  // Add a UI element according to a dictionary passed from the UI HTML.
  void AddUiElementFromDict(const base::DictionaryValue& dict);

  // Update an existing element with new properties.
  void UpdateUiElementFromDict(const base::DictionaryValue& dict);

  void RemoveUiElement(int element_id);

  // Add an animation to the scene, on element |element_id|.
  void AddAnimation(int element_id, std::unique_ptr<Animation>& animation);

  // Add an animation according to a dictionary passed from the UI HTML.
  void AddAnimationFromDict(const base::DictionaryValue& dict,
                            int64_t time_in_micro);

  // Remove |animation_id| from element |element_id|.
  void RemoveAnimation(int element_id, int animation_id);

  void UpdateBackgroundFromDict(const base::DictionaryValue& dict);

  // Update the positions of all elements in the scene, according to active
  // animations, desired screen tilt and time.  The units of time are
  // arbitrary, but must match the unit used in animations.
  void UpdateTransforms(float screen_tilt, int64_t time_in_micro);

  // Handle a batch of commands passed from the UI HTML.
  void HandleCommands(std::unique_ptr<base::ListValue> commands,
                      int64_t time_in_micro);

  const std::vector<std::unique_ptr<ContentRectangle>>& GetUiElements() const;

  ContentRectangle* GetUiElementById(int element_id);

  const Colorf& GetBackgroundColor();
  float GetBackgroundDistance();

 private:
  void ApplyRecursiveTransforms(const ContentRectangle& element,
                                ReversibleTransform* transform,
                                float* opacity);
  void ApplyDictToElement(const base::DictionaryValue& dict,
                          ContentRectangle* element);

  std::vector<std::unique_ptr<ContentRectangle>> ui_elements_;
  ContentRectangle* content_element_ = nullptr;
  Colorf background_color_ = {0.1f, 0.1f, 0.1f, 1.0f};
  float background_distance_ = 10.0f;

  DISALLOW_COPY_AND_ASSIGN(UiScene);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_SCENE_H_

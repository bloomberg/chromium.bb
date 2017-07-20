// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_H_
#define CHROME_BROWSER_VR_UI_SCENE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/elements/ui_element_debug_id.h"
#include "third_party/skia/include/core/SkColor.h"

namespace base {
class ListValue;
class TimeTicks;
}  // namespace base

namespace cc {
class Animation;
}  // namespace cc

namespace vr {

class UiElement;

class UiScene {
 public:
  enum Command {
    ADD_ELEMENT,
    UPDATE_ELEMENT,
    REMOVE_ELEMENT,
    ADD_ANIMATION,
    REMOVE_ANIMATION,
    CONFIGURE_SCENE,
  };

  UiScene();
  virtual ~UiScene();

  void AddUiElement(std::unique_ptr<UiElement> element);

  void RemoveUiElement(int element_id);

  // Add an animation to the scene, on element |element_id|.
  void AddAnimation(int element_id, std::unique_ptr<cc::Animation> animation);

  // Remove |animation_id| from element |element_id|.
  void RemoveAnimation(int element_id, int animation_id);

  // Handles per-frame updates, giving each element the opportunity to update,
  // if necessary (eg, for animations). NB: |current_time| is the shared,
  // absolute begin frame time.
  void OnBeginFrame(const base::TimeTicks& current_time);

  // This function gets called just before rendering the elements in the
  // frame lifecycle. After this function, no element should be dirtied.
  void PrepareToDraw();

  // Handle a batch of commands passed from the UI HTML.
  void HandleCommands(std::unique_ptr<base::ListValue> commands,
                      const base::TimeTicks& current_time);

  const std::vector<std::unique_ptr<UiElement>>& GetUiElements() const;

  UiElement* GetUiElementById(int element_id) const;
  UiElement* GetUiElementByDebugId(UiElementDebugId debug_id) const;

  std::vector<const UiElement*> GetWorldElements() const;
  std::vector<const UiElement*> GetOverlayElements() const;
  std::vector<const UiElement*> GetHeadLockedElements() const;
  bool HasVisibleHeadLockedElements() const;

  float background_distance() const { return background_distance_; }
  void set_background_distance(float d) { background_distance_ = d; }
  SkColor background_color() const { return background_color_; }
  void set_background_color(SkColor color) { background_color_ = color; }

  bool web_vr_rendering_enabled() const { return webvr_rendering_enabled_; }
  void set_web_vr_rendering_enabled(bool enabled) {
    webvr_rendering_enabled_ = enabled;
  }
  bool reticle_rendering_enabled() const { return reticle_rendering_enabled_; }
  void set_reticle_rendering_enabled(bool enabled) {
    reticle_rendering_enabled_ = enabled;
  }

  void OnGLInitialized();

 private:
  void Animate(const base::TimeTicks& current_time);
  void ApplyRecursiveTransforms(UiElement* element);

  std::vector<std::unique_ptr<UiElement>> ui_elements_;
  UiElement* content_element_ = nullptr;
  ColorScheme::Mode mode_ = ColorScheme::kModeNormal;

  float background_distance_ = 10.0f;
  SkColor background_color_ = 0;
  bool webvr_rendering_enabled_ = false;
  bool reticle_rendering_enabled_ = true;
  bool gl_initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(UiScene);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_H_

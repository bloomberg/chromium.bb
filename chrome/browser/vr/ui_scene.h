// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SCENE_H_
#define CHROME_BROWSER_VR_UI_SCENE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_iterator.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "third_party/skia/include/core/SkColor.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace gfx {
class Transform;
}  // namespace gfx

namespace vr {

class UiElement;

class UiScene {
 public:
  UiScene();
  ~UiScene();

  void AddUiElement(UiElementName parent, std::unique_ptr<UiElement> element);

  std::unique_ptr<UiElement> RemoveUiElement(int element_id);

  // Handles per-frame updates, giving each element the opportunity to update,
  // if necessary (eg, for animations). NB: |current_time| is the shared,
  // absolute begin frame time.
  // Returns true if *anything* was updated.
  bool OnBeginFrame(const base::TimeTicks& current_time,
                    const gfx::Transform& head_pose);

  // Returns true if any textures were redrawn.
  bool UpdateTextures();

  UiElement& root_element();

  UiElement* GetUiElementById(int element_id) const;
  UiElement* GetUiElementByName(UiElementName name) const;

  typedef std::vector<const UiElement*> Elements;

  Elements GetVisibleElementsToDraw() const;
  Elements GetVisibleWebVrOverlayElementsToDraw() const;
  Elements GetPotentiallyVisibleElements() const;

  float background_distance() const { return background_distance_; }
  void set_background_distance(float d) { background_distance_ = d; }

  void set_dirty() { is_dirty_ = true; }

  void OnGlInitialized(SkiaSurfaceProvider* provider);

  SkiaSurfaceProvider* SurfaceProviderForTesting() { return provider_; }

 private:
  void InitializeElement(UiElement* element);

  std::unique_ptr<UiElement> root_element_;

  float background_distance_ = 10.0f;
  bool gl_initialized_ = false;
  bool initialized_scene_ = false;

  // TODO(mthiesse): Convert everything that manipulates UI elements to
  // bindings. Don't allow any code to go in and manipulate UI elements outside
  // of bindings so that we can do a single pass and update everything and
  // easily compute dirtiness.
  bool is_dirty_ = false;

  SkiaSurfaceProvider* provider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UiScene);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SCENE_H_

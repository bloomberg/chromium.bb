// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATIONS_H_
#define ASH_WM_WINDOW_ANIMATIONS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/window_animations.h"

namespace aura {
class Window;
}
namespace ui {
class Layer;
class LayerTreeOwner;
}
namespace views {
}

// This is only for animations specific to Ash. For window animations shared
// with desktop Chrome, see ui/views/corewm/window_animations.h.
namespace ash {

// Direction for ash-specific window animations used in workspaces and
// lock/unlock animations.
enum LayerScaleAnimationDirection {
  LAYER_SCALE_ANIMATION_ABOVE,
  LAYER_SCALE_ANIMATION_BELOW,
};

// Amount of time for the cross fade animation.
extern const int kCrossFadeDurationMS;

// Implementation of cross fading. Window is the window being cross faded. It
// should be at the target bounds. |old_layer_owner| contains the previous layer
// from |window|.  |tween_type| specifies the tween type of the cross fade
// animation.
ASH_EXPORT base::TimeDelta CrossFadeAnimation(
    aura::Window* window,
    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner,
    gfx::Tween::Type tween_type);

ASH_EXPORT bool AnimateOnChildWindowVisibilityChanged(aura::Window* window,
                                                      bool visible);

// Creates vector of animation sequences that lasts for |duration| and changes
// brightness and grayscale to |target_value|. Caller takes ownership of
// returned LayerAnimationSequence objects.
ASH_EXPORT std::vector<ui::LayerAnimationSequence*>
CreateBrightnessGrayscaleAnimationSequence(float target_value,
                                           base::TimeDelta duration);

// Applies scale related to the specified AshWindowScaleType.
ASH_EXPORT void SetTransformForScaleAnimation(
    ui::Layer* layer,
    LayerScaleAnimationDirection type);

// Returns the approximate bounds to which |window| will be animated when it
// is minimized. The bounds are approximate because the minimize animation
// involves rotation.
ASH_EXPORT gfx::Rect GetMinimizeAnimationTargetBoundsInScreen(
    aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_WINDOW_ANIMATIONS_H_

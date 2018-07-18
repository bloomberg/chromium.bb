// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_ANIMATION_UTIL_H_
#define ASH_ASSISTANT_UTIL_ANIMATION_UTIL_H_

#include <memory>

#include "ui/gfx/animation/tween.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ui {
class LayerAnimationElement;
class LayerAnimationSequence;
}  // namespace ui

namespace ash {
namespace assistant {
namespace util {

// Creates a LayerAnimationSequence containing the specified
// LayerAnimationElements. The method caller assumes ownership of the
// returned pointer.
ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<ui::LayerAnimationElement> a,
    std::unique_ptr<ui::LayerAnimationElement> b = nullptr);

// Creates a LayerAnimationSequence containing the specified
// |layer_animation_element| to be started after the given |delay|. The method
// caller assumes ownership of the returned pointer.
ui::LayerAnimationSequence* CreateLayerAnimationSequenceWithDelay(
    std::unique_ptr<ui::LayerAnimationElement> layer_animation_element,
    const base::TimeDelta& delay);

// Creates a LayerAnimationElement to animate opacity with the given parameters.
std::unique_ptr<ui::LayerAnimationElement> CreateOpacityElement(
    float opacity,
    const base::TimeDelta& duration,
    const gfx::Tween::Type& tween = gfx::Tween::Type::LINEAR);

// Creates a LayerAnimationElement to animate transform with the given
// parameters.
std::unique_ptr<ui::LayerAnimationElement> CreateTransformElement(
    const gfx::Transform& transform,
    const base::TimeDelta& duration,
    const gfx::Tween::Type& tween = gfx::Tween::Type::LINEAR);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_ANIMATION_UTIL_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_ANIMATION_UTIL_H_
#define ASH_ASSISTANT_UTIL_ANIMATION_UTIL_H_

#include <memory>

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
// |layer_animation_element|. The method caller assumes ownership of the
// returned pointer.
ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<ui::LayerAnimationElement> layer_animation_element);

// Creates a LayerAnimationSequence containing the specified
// |layer_animation_element| to be started after the given |delay|. The method
// caller assumes ownership of the returned pointer.
ui::LayerAnimationSequence* CreateLayerAnimationSequenceWithDelay(
    std::unique_ptr<ui::LayerAnimationElement> layer_animation_element,
    base::TimeDelta delay);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_ANIMATION_UTIL_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/animation_util.h"

#include "base/time/time.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"

namespace ash {
namespace assistant {
namespace util {

ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<ui::LayerAnimationElement> layer_animation_element) {
  return CreateLayerAnimationSequenceWithDelay(
      std::move(layer_animation_element),
      /*delay=*/base::TimeDelta());
}

ui::LayerAnimationSequence* CreateLayerAnimationSequenceWithDelay(
    std::unique_ptr<ui::LayerAnimationElement> layer_animation_element,
    base::TimeDelta delay) {
  DCHECK(delay.InMilliseconds() >= 0);

  ui::LayerAnimationSequence* layer_animation_sequence =
      new ui::LayerAnimationSequence();

  if (!delay.is_zero()) {
    layer_animation_sequence->AddElement(
        ui::LayerAnimationElement::CreatePauseElement(
            layer_animation_element->properties(), delay));
  }

  layer_animation_sequence->AddElement(std::move(layer_animation_element));

  return layer_animation_sequence;
}

}  // namespace util
}  // namespace assistant
}  // namespace ash

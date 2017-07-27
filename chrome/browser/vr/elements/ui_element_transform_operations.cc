// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element_transform_operations.h"

#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

UiElementTransformOperations::UiElementTransformOperations() {
  operations_.AppendTranslate(0, 0, 0);
  operations_.AppendRotate(1, 0, 0, 0);
  operations_.AppendScale(1, 1, 1);
}

UiElementTransformOperations::~UiElementTransformOperations() {}

void UiElementTransformOperations::SetTranslate(float x, float y, float z) {
  cc::TransformOperation& op = operations_.at(UiElement::kTranslateIndex);
  op.translate = {x, y, z};
  op.Bake();
}

void UiElementTransformOperations::SetRotate(float x,
                                             float y,
                                             float z,
                                             float degrees) {
  cc::TransformOperation& op = operations_.at(UiElement::kRotateIndex);
  op.rotate.axis = {x, y, z};
  op.rotate.angle = degrees;
  op.Bake();
}

void UiElementTransformOperations::SetScale(float x, float y, float z) {
  cc::TransformOperation& op = operations_.at(UiElement::kScaleIndex);
  op.scale = {x, y, z};
  op.Bake();
}

}  // namespace vr

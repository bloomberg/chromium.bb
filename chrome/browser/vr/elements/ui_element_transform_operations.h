// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_TRANSFORM_OPERATIONS_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_TRANSFORM_OPERATIONS_H_

#include "base/macros.h"
#include "cc/animation/transform_operations.h"

namespace vr {

// This class should be used solely for setting translate / rotate / scale, etc
// on an element in unison to prevent those from triggering separate animations.
// It is also meant to hide the implementation detail of how we arrange our
// transformation operations in UiElement instances. By and large, you should be
// able to call the corresponding UiElement::SetXXX functions.
class UiElementTransformOperations {
 public:
  UiElementTransformOperations();
  ~UiElementTransformOperations();

  void SetTranslate(float x, float y, float z);
  void SetRotate(float x, float y, float z, float degrees);
  void SetScale(float x, float y, float z);

  const cc::TransformOperations& operations() const { return operations_; }

 private:
  cc::TransformOperations operations_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_TRANSFORM_OPERATIONS_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/button.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/transform_operation.h"
#include "cc/animation/transform_operations.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

TEST(Button, HoverTest) {
  auto button =
      base::MakeUnique<Button>(base::Callback<void()>(), kPhaseForeground, 1.0,
                               1.0, vector_icons::kMicrophoneIcon);

  cc::TransformOperation foreground_op =
      button->foreground()->GetTargetTransform().at(UiElement::kTranslateIndex);
  cc::TransformOperation background_op =
      button->background()->GetTargetTransform().at(UiElement::kTranslateIndex);

  button->OnHoverEnter(gfx::PointF(0.5f, 0.5f));
  cc::TransformOperation foreground_op_hover =
      button->foreground()->GetTargetTransform().at(UiElement::kTranslateIndex);
  cc::TransformOperation background_op_hover =
      button->background()->GetTargetTransform().at(UiElement::kTranslateIndex);

  EXPECT_TRUE(foreground_op_hover.translate.z - foreground_op.translate.z >
              0.f);
  EXPECT_TRUE(background_op_hover.translate.z - background_op.translate.z >
              0.f);
}

}  // namespace vr

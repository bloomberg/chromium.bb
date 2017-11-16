// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/shadow.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(Shadow, ShadowPaddingGrows) {
  UiScene scene;
  auto rect = base::MakeUnique<Rect>();
  auto* rect_ptr = rect.get();
  rect->SetSize(2.0, 2.0);

  auto shadow = base::MakeUnique<Shadow>();
  auto* shadow_ptr = shadow.get();
  shadow->AddChild(std::move(rect));
  scene.AddUiElement(kRoot, std::move(shadow));

  scene.OnBeginFrame(MsToTicks(0), kForwardVector);
  float old_x_padding = shadow_ptr->x_padding();
  float old_y_padding = shadow_ptr->y_padding();
  EXPECT_LE(0.0f, old_x_padding);
  EXPECT_LE(0.0f, old_y_padding);

  rect_ptr->SetTranslate(0, 0, 0.15);
  scene.OnBeginFrame(MsToTicks(0), kForwardVector);
  float new_x_padding = shadow_ptr->x_padding();
  float new_y_padding = shadow_ptr->y_padding();
  EXPECT_LE(old_x_padding, new_x_padding);
  EXPECT_LE(old_y_padding, new_y_padding);
}

}  // namespace vr

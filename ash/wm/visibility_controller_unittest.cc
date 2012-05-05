// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/visibility_controller.h"

#include "ash/test/ash_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"

namespace ash {
namespace internal {

typedef test::AshTestBase VisibilityControllerTest;

// Hiding a window in an animatable container should not hide the window's layer
// immediately.
TEST_F(VisibilityControllerTest, AnimateHideDoesntHideWindowLayer) {
  // We cannot disable animations for this test.
  ui::LayerAnimator::set_disable_animations_for_test(false);

  scoped_ptr<aura::Window> container(
      aura::test::CreateTestWindowWithId(-1, NULL));
  SetChildWindowVisibilityChangesAnimated(container.get());

  aura::test::TestWindowDelegate d;
  scoped_ptr<aura::Window> animatable(
      aura::test::CreateTestWindowWithDelegate(
          &d, -2, gfx::Rect(0, 0, 50, 50), container.get()));
  scoped_ptr<aura::Window> non_animatable(
      aura::test::CreateTestWindowWithDelegateAndType(
          &d, aura::client::WINDOW_TYPE_CONTROL, -3, gfx::Rect(51, 51, 50, 50),
          container.get()));
  EXPECT_TRUE(animatable->IsVisible());
  EXPECT_TRUE(animatable->layer()->visible());
  animatable->Hide();
  EXPECT_FALSE(animatable->IsVisible());
  EXPECT_TRUE(animatable->layer()->visible());

  EXPECT_TRUE(non_animatable->IsVisible());
  EXPECT_TRUE(non_animatable->layer()->visible());
  non_animatable->Hide();
  EXPECT_FALSE(non_animatable->IsVisible());
  EXPECT_FALSE(non_animatable->layer()->visible());
}

}  // namespace internal
}  // namespace ash

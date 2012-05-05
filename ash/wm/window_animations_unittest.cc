// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/animation/animation_container_element.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"

namespace ash {
namespace internal {

typedef ash::test::AshTestBase WindowAnimationsWorkspaceTest;

TEST_F(WindowAnimationsWorkspaceTest, HideShow) {
  aura::Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, default_container));
  window->Show();
  EXPECT_TRUE(window->layer()->visible());
  // Hiding.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_HIDE);
  ash::internal::AnimateOnChildWindowVisibilityChanged(
      window.get(), false);
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->visible());
  // Showing.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_SHOW);
  ash::internal::AnimateOnChildWindowVisibilityChanged(
      window.get(), true);
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_TRUE(window->layer()->visible());
  // Stays showing.
  ui::AnimationContainerElement* element =
      static_cast<ui::AnimationContainerElement*>(
      window->layer()->GetAnimator());
  element->Step(base::TimeTicks::Now() +
                base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_TRUE(window->layer()->visible());
}

TEST_F(WindowAnimationsWorkspaceTest, ShowHide) {
  aura::Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, default_container));
  window->Show();
  EXPECT_TRUE(window->layer()->visible());
  // Showing -- should be a no-op.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_SHOW);
  ash::internal::AnimateOnChildWindowVisibilityChanged(
      window.get(), true);
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_TRUE(window->layer()->visible());
  // Hiding.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_HIDE);
  ash::internal::AnimateOnChildWindowVisibilityChanged(
      window.get(), false);
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->visible());
  // Stays hidden.
  ui::AnimationContainerElement* element =
      static_cast<ui::AnimationContainerElement*>(
      window->layer()->GetAnimator());
  element->Step(base::TimeTicks::Now() +
                base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->visible());
}

}  // namespace internal
}  // namespace ash

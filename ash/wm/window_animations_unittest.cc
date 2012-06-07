// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/time.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/animation/animation_container_element.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"

using aura::Window;
using ui::Layer;

namespace ash {
namespace internal {

typedef ash::test::AshTestBase WindowAnimationsTest;

TEST_F(WindowAnimationsTest, HideShow) {
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
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());
  // Showing.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_SHOW);
  ash::internal::AnimateOnChildWindowVisibilityChanged(
      window.get(), true);
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_TRUE(window->layer()->visible());
  // Stays showing.
  ui::AnimationContainerElement* element =
      static_cast<ui::AnimationContainerElement*>(
      window->layer()->GetAnimator());
  element->Step(base::TimeTicks::Now() +
                base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_TRUE(window->layer()->visible());
}

TEST_F(WindowAnimationsTest, ShowHide) {
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
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_TRUE(window->layer()->visible());
  // Hiding.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_HIDE);
  ash::internal::AnimateOnChildWindowVisibilityChanged(
      window.get(), false);
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());
  // Stays hidden.
  ui::AnimationContainerElement* element =
      static_cast<ui::AnimationContainerElement*>(
      window->layer()->GetAnimator());
  element->Step(base::TimeTicks::Now() +
                base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());
}

TEST_F(WindowAnimationsTest, LayerTargetVisibility) {
  aura::Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, default_container));

  // Layer target visibility changes according to Show/Hide.
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  window->Hide();
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
}

TEST_F(WindowAnimationsTest, CrossFadeToBounds) {
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window(
      aura::test::CreateTestWindowWithId(0, default_container));
  window->SetBounds(gfx::Rect(5, 10, 320, 240));
  window->Show();

  Layer* old_layer = window->layer();
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());

  // Cross fade to a larger size, as in a maximize animation.
  CrossFadeToBounds(window.get(), gfx::Rect(0, 0, 640, 480));
  // Window's layer has been replaced.
  EXPECT_NE(old_layer, window->layer());
  // Original layer stays opaque and stretches to new size.
  EXPECT_EQ(1.0f, old_layer->GetTargetOpacity());
  EXPECT_EQ("5,10 320x240", old_layer->bounds().ToString());
  ui::Transform grow_transform;
  grow_transform.ConcatScale(640.f / 320.f, 480.f / 240.f);
  grow_transform.ConcatTranslate(-5.f, -10.f);
  EXPECT_EQ(grow_transform, old_layer->GetTargetTransform());
  // New layer animates in to the identity transform.
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(ui::Transform(), window->layer()->GetTargetTransform());

  // Allow the animation observer to delete itself.
  RunAllPendingInMessageLoop();

  // Cross fade to a smaller size, as in a restore animation.
  old_layer = window->layer();
  CrossFadeToBounds(window.get(), gfx::Rect(5, 10, 320, 240));
  // Again, window layer has been replaced.
  EXPECT_NE(old_layer, window->layer());
  // Original layer fades out and stretches down to new size.
  EXPECT_EQ(0.0f, old_layer->GetTargetOpacity());
  EXPECT_EQ("0,0 640x480", old_layer->bounds().ToString());
  ui::Transform shrink_transform;
  shrink_transform.ConcatScale(320.f / 640.f, 240.f / 480.f);
  shrink_transform.ConcatTranslate(5.f, 10.f);
  EXPECT_EQ(shrink_transform, old_layer->GetTargetTransform());
  // New layer animates in to the identity transform.
  EXPECT_EQ(1.0f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(ui::Transform(), window->layer()->GetTargetTransform());

  RunAllPendingInMessageLoop();
}

TEST_F(WindowAnimationsTest, GetCrossFadeDuration) {
  gfx::Rect empty;
  gfx::Rect screen(0, 0, 1000, 500);

  // No change takes no time.
  EXPECT_EQ(0, GetCrossFadeDuration(empty, empty).InMilliseconds());
  EXPECT_EQ(0, GetCrossFadeDuration(screen, screen).InMilliseconds());

  // Small changes are fast.
  gfx::Rect almost_screen(10, 10, 900, 400);
  EXPECT_EQ(220, GetCrossFadeDuration(almost_screen, screen).InMilliseconds());
  EXPECT_EQ(220, GetCrossFadeDuration(screen, almost_screen).InMilliseconds());

  // Large changes are slow.
  gfx::Rect small(10, 10, 100, 100);
  EXPECT_EQ(380, GetCrossFadeDuration(small, screen).InMilliseconds());
  EXPECT_EQ(380, GetCrossFadeDuration(screen, small).InMilliseconds());

  // Medium changes take medium time.
  gfx::Rect half_screen(10, 10, 500, 250);
  EXPECT_EQ(300, GetCrossFadeDuration(half_screen, screen).InMilliseconds());
  EXPECT_EQ(300, GetCrossFadeDuration(screen, half_screen).InMilliseconds());

  // Change is based on width.
  gfx::Rect narrow(10, 10, 100, 500);
  gfx::Rect wide(10, 10, 900, 500);
  EXPECT_EQ(380, GetCrossFadeDuration(narrow, screen).InMilliseconds());
  EXPECT_EQ(220, GetCrossFadeDuration(wide, screen).InMilliseconds());
}

}  // namespace internal
}  // namespace ash

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/workspace_controller.h"
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

class WindowAnimationsTest : public ash::test::AshTestBase {
 public:
  WindowAnimationsTest() {}

  virtual void TearDown() OVERRIDE {
    ui::LayerAnimator::set_disable_animations_for_test(true);
    AshTestBase::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowAnimationsTest);
};

TEST_F(WindowAnimationsTest, HideShow) {
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, NULL));
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
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, NULL));
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

TEST_F(WindowAnimationsTest, HideShowBrightnessGrayscaleAnimation) {
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, NULL));
  window->Show();
  EXPECT_TRUE(window->layer()->visible());

  // Hiding.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE);
  ash::internal::AnimateOnChildWindowVisibilityChanged(
      window.get(), false);
  EXPECT_EQ(0.0f, window->layer()->GetTargetOpacity());
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  EXPECT_FALSE(window->layer()->visible());

  // Showing.
  SetWindowVisibilityAnimationType(
      window.get(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE);
  ash::internal::AnimateOnChildWindowVisibilityChanged(
      window.get(), true);
  EXPECT_EQ(0.0f, window->layer()->GetTargetBrightness());
  EXPECT_EQ(0.0f, window->layer()->GetTargetGrayscale());
  EXPECT_TRUE(window->layer()->visible());

  // Stays shown.
  ui::AnimationContainerElement* element =
      static_cast<ui::AnimationContainerElement*>(
      window->layer()->GetAnimator());
  element->Step(base::TimeTicks::Now() +
                base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(0.0f, window->layer()->GetTargetBrightness());
  EXPECT_EQ(0.0f, window->layer()->GetTargetGrayscale());
  EXPECT_TRUE(window->layer()->visible());
}

TEST_F(WindowAnimationsTest, LayerTargetVisibility) {
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, NULL));

  // Layer target visibility changes according to Show/Hide.
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  window->Hide();
  EXPECT_FALSE(window->layer()->GetTargetVisibility());
  window->Show();
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
}

TEST_F(WindowAnimationsTest, CrossFadeToBounds) {
  ui::LayerAnimator::set_disable_animations_for_test(false);

  scoped_ptr<Window> window(
      aura::test::CreateTestWindowWithId(0, NULL));
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

  // Run the animations to completion.
  static_cast<ui::AnimationContainerElement*>(old_layer->GetAnimator())->Step(
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
  static_cast<ui::AnimationContainerElement*>(window->layer()->GetAnimator())->
      Step(base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));

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

  static_cast<ui::AnimationContainerElement*>(old_layer->GetAnimator())->Step(
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
  static_cast<ui::AnimationContainerElement*>(window->layer()->GetAnimator())->
      Step(base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
}

TEST_F(WindowAnimationsTest, GetCrossFadeDuration) {
  if (WorkspaceController::IsWorkspace2Enabled())
    return;

  gfx::Rect empty;
  gfx::Rect screen(0, 0, 1000, 500);

  // No change takes no time.
  EXPECT_EQ(0, GetCrossFadeDuration(empty, empty).InMilliseconds());
  EXPECT_EQ(0, GetCrossFadeDuration(screen, screen).InMilliseconds());

  // Small changes are fast.
  const int kMinimum = 100;
  const int kRange = 300;
  gfx::Rect almost_screen(10, 10, 1000, 450);  // 90% of screen area
  EXPECT_EQ(kMinimum + kRange / 10,
           GetCrossFadeDuration(almost_screen, screen).InMilliseconds());
  EXPECT_EQ(kMinimum + kRange / 10,
            GetCrossFadeDuration(screen, almost_screen).InMilliseconds());

  // Large changes are slow.
  gfx::Rect small(10, 10, 100, 500);  // 10% of screen area
  EXPECT_EQ(kMinimum + kRange * 9 / 10,
            GetCrossFadeDuration(small, screen).InMilliseconds());
  EXPECT_EQ(kMinimum + kRange * 9 / 10,
            GetCrossFadeDuration(screen, small).InMilliseconds());

  // Medium changes take medium time.
  gfx::Rect half_screen(10, 10, 500, 250);
  EXPECT_EQ(kMinimum + kRange * 3 / 4,
            GetCrossFadeDuration(half_screen, screen).InMilliseconds());
  EXPECT_EQ(kMinimum + kRange * 3 / 4,
            GetCrossFadeDuration(screen, half_screen).InMilliseconds());
}

}  // namespace internal
}  // namespace ash

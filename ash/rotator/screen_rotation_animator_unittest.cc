// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animator.h"
#include "ash/common/wm_shell.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/rotator/test/screen_rotation_animator_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

display::Display::Rotation GetDisplayRotation(int64_t display_id) {
  return Shell::GetInstance()
      ->display_manager()
      ->GetDisplayInfo(display_id)
      .GetActiveRotation();
}

void SetDisplayRotation(int64_t display_id,
                        display::Display::Rotation rotation) {
  Shell::GetInstance()->display_manager()->SetDisplayRotation(
      display_id, rotation,
      display::Display::RotationSource::ROTATION_SOURCE_USER);
}

class AnimationObserver : public ScreenRotationAnimatorObserver {
 public:
  AnimationObserver() {}

  bool notified() const { return notified_; }

  void OnScreenRotationAnimationFinished(
      ScreenRotationAnimator* animator) override {
    notified_ = true;
  }

 private:
  bool notified_ = false;

  DISALLOW_COPY_AND_ASSIGN(AnimationObserver);
};

}  // namespace

class ScreenRotationAnimatorTest : public test::AshTestBase {
 public:
  ScreenRotationAnimatorTest() {}
  ~ScreenRotationAnimatorTest() override {}

  // AshTestBase:
  void SetUp() override;

 protected:
  int64_t display_id() const { return display_.id(); }

  ScreenRotationAnimator* animator() { return animator_.get(); }

  test::ScreenRotationAnimatorTestApi* test_api() { return test_api_.get(); }

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> non_zero_duration_mode_;

 private:
  display::Display display_;

  std::unique_ptr<ScreenRotationAnimator> animator_;

  std::unique_ptr<test::ScreenRotationAnimatorTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationAnimatorTest);
};

void ScreenRotationAnimatorTest::SetUp() {
  AshTestBase::SetUp();

  display_ = display::Screen::GetScreen()->GetPrimaryDisplay();
  animator_ = base::MakeUnique<ScreenRotationAnimator>(display_.id());
  test_api_ =
      base::MakeUnique<test::ScreenRotationAnimatorTestApi>(animator_.get());
  test_api()->DisableAnimationTimers();
  non_zero_duration_mode_ =
      base::MakeUnique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
}

TEST_F(ScreenRotationAnimatorTest, ShouldNotifyObserver) {
  // TODO(wutao): needs GetDisplayInfo http://crbug.com/622480.
  if (WmShell::Get()->IsRunningInMash()) {
    ASSERT_TRUE(WmShell::Get()->GetDisplayInfo(display_id()).id() !=
                display_id());
    return;
  }

  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  AnimationObserver observer;
  animator()->AddScreenRotationAnimatorObserver(&observer);
  EXPECT_FALSE(observer.notified());

  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_FALSE(observer.notified());

  test_api()->CompleteAnimations();
  EXPECT_TRUE(observer.notified());
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  animator()->RemoveScreenRotationAnimatorObserver(&observer);
}

TEST_F(ScreenRotationAnimatorTest, ShouldNotifyObserverOnce) {
  // TODO(wutao): needs GetDisplayInfo http://crbug.com/622480.
  if (WmShell::Get()->IsRunningInMash()) {
    ASSERT_TRUE(WmShell::Get()->GetDisplayInfo(display_id()).id() !=
                display_id());
    return;
  }

  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  AnimationObserver observer;
  animator()->AddScreenRotationAnimatorObserver(&observer);
  EXPECT_FALSE(observer.notified());

  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_FALSE(observer.notified());

  animator()->Rotate(display::Display::ROTATE_180,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_FALSE(observer.notified());

  test_api()->CompleteAnimations();
  EXPECT_TRUE(observer.notified());
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  animator()->RemoveScreenRotationAnimatorObserver(&observer);
}

TEST_F(ScreenRotationAnimatorTest, RotatesToDifferentRotation) {
  // TODO(wutao): needs GetDisplayInfo http://crbug.com/622480.
  if (WmShell::Get()->IsRunningInMash()) {
    ASSERT_TRUE(WmShell::Get()->GetDisplayInfo(display_id()).id() !=
                display_id());
    return;
  }

  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
}

TEST_F(ScreenRotationAnimatorTest, ShouldNotRotateTheSameRotation) {
  // TODO(wutao): needs GetDisplayInfo http://crbug.com/622480.
  if (WmShell::Get()->IsRunningInMash()) {
    ASSERT_TRUE(WmShell::Get()->GetDisplayInfo(display_id()).id() !=
                display_id());
    return;
  }

  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_0,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_FALSE(test_api()->HasActiveAnimations());
}

// Simulates the situation that if there is a new rotation request during
// animation, it should stop the animation immediately and add the new rotation
// request to the |last_pending_request_|.
TEST_F(ScreenRotationAnimatorTest, RotatesDuringRotation) {
  // TODO(wutao): needs GetDisplayInfo http://crbug.com/622480.
  if (WmShell::Get()->IsRunningInMash()) {
    ASSERT_TRUE(WmShell::Get()->GetDisplayInfo(display_id()).id() !=
                display_id());
    return;
  }

  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  animator()->Rotate(display::Display::ROTATE_180,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  EXPECT_EQ(display::Display::ROTATE_180, GetDisplayRotation(display_id()));
}

// If there are multiple requests queued during animation, it should process the
// last request and finish the rotation animation.
TEST_F(ScreenRotationAnimatorTest, ShouldCompleteAnimations) {
  // TODO(wutao): needs GetDisplayInfo http://crbug.com/622480.
  if (WmShell::Get()->IsRunningInMash()) {
    ASSERT_TRUE(WmShell::Get()->GetDisplayInfo(display_id()).id() !=
                display_id());
    return;
  }

  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  animator()->Rotate(display::Display::ROTATE_180,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  animator()->Rotate(display::Display::ROTATE_270,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
  EXPECT_EQ(display::Display::ROTATE_270, GetDisplayRotation(display_id()));
}

}  // namespace ash

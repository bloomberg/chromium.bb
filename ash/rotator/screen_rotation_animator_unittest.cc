// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animator.h"

#include "ash/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/rotator/test/screen_rotation_animator_test_api.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/test/ash_test_base.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

display::Display::Rotation GetDisplayRotation(int64_t display_id) {
  return Shell::Get()
      ->display_manager()
      ->GetDisplayInfo(display_id)
      .GetActiveRotation();
}

void SetDisplayRotation(int64_t display_id,
                        display::Display::Rotation rotation) {
  Shell::Get()->display_manager()->SetDisplayRotation(
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

class TestScreenRotationAnimator : public ScreenRotationAnimator {
 public:
  TestScreenRotationAnimator(int64_t display_id, const base::Closure& callback);
  ~TestScreenRotationAnimator() override {}

 private:
  CopyCallback CreateAfterCopyCallback(
      std::unique_ptr<ScreenRotationRequest> rotation_request) override;

  void IntersectBefore(CopyCallback next_callback,
                       std::unique_ptr<cc::CopyOutputResult> result);

  base::Closure intersect_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestScreenRotationAnimator);
};

TestScreenRotationAnimator::TestScreenRotationAnimator(
    int64_t display_id,
    const base::Closure& callback)
    : ScreenRotationAnimator(display_id), intersect_callback_(callback) {}

ScreenRotationAnimator::CopyCallback
TestScreenRotationAnimator::CreateAfterCopyCallback(
    std::unique_ptr<ScreenRotationRequest> rotation_request) {
  CopyCallback next_callback = ScreenRotationAnimator::CreateAfterCopyCallback(
      std::move(rotation_request));
  return base::Bind(&TestScreenRotationAnimator::IntersectBefore,
                    base::Unretained(this), next_callback);
}

void TestScreenRotationAnimator::IntersectBefore(
    CopyCallback next_callback,
    std::unique_ptr<cc::CopyOutputResult> result) {
  intersect_callback_.Run();
  next_callback.Run(std::move(result));
}

}  // namespace

class ScreenRotationAnimatorTest : public test::AshTestBase {
 public:
  ScreenRotationAnimatorTest() {}
  ~ScreenRotationAnimatorTest() override {}

  // AshTestBase:
  void SetUp() override;

  void RemoveSecondaryDisplay(const std::string& specs);

 protected:
  int64_t display_id() const { return display_.id(); }

  TestScreenRotationAnimator* animator() { return animator_.get(); }

  void SetScreenRotationAnimator(int64_t display_id,
                                 const base::Closure& callback);

  test::ScreenRotationAnimatorTestApi* test_api() { return test_api_.get(); }

  void WaitForCopyCallback();

  std::unique_ptr<base::RunLoop> run_loop_;

 private:
  display::Display display_;

  std::unique_ptr<TestScreenRotationAnimator> animator_;

  std::unique_ptr<test::ScreenRotationAnimatorTestApi> test_api_;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> non_zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationAnimatorTest);
};

void ScreenRotationAnimatorTest::RemoveSecondaryDisplay(
    const std::string& specs) {
  UpdateDisplay(specs);
  run_loop_->QuitWhenIdle();
}

void ScreenRotationAnimatorTest::SetUp() {
  AshTestBase::SetUp();

  display_ = display::Screen::GetScreen()->GetPrimaryDisplay();
  run_loop_ = base::MakeUnique<base::RunLoop>();
  SetScreenRotationAnimator(display_.id(), run_loop_->QuitWhenIdleClosure());
  non_zero_duration_mode_ =
      base::MakeUnique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
}

void ScreenRotationAnimatorTest::SetScreenRotationAnimator(
    int64_t display_id,
    const base::Closure& callback) {
  animator_ =
      base::MakeUnique<TestScreenRotationAnimator>(display_id, callback);
  test_api_ =
      base::MakeUnique<test::ScreenRotationAnimatorTestApi>(animator_.get());
  test_api()->DisableAnimationTimers();
}

void ScreenRotationAnimatorTest::WaitForCopyCallback() {
  run_loop_->Run();
}

TEST_F(ScreenRotationAnimatorTest, ShouldNotifyObserver) {
  // TODO(wutao): needs GetDisplayInfo http://crbug.com/622480.
  if (Shell::GetAshConfig() == Config::MASH) {
    ASSERT_TRUE(ShellPort::Get()->GetDisplayInfo(display_id()).id() !=
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
  if (Shell::GetAshConfig() == Config::MASH) {
    ASSERT_TRUE(ShellPort::Get()->GetDisplayInfo(display_id()).id() !=
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
  if (Shell::GetAshConfig() == Config::MASH) {
    ASSERT_TRUE(ShellPort::Get()->GetDisplayInfo(display_id()).id() !=
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
  if (Shell::GetAshConfig() == Config::MASH) {
    ASSERT_TRUE(ShellPort::Get()->GetDisplayInfo(display_id()).id() !=
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
  if (Shell::GetAshConfig() == Config::MASH) {
    ASSERT_TRUE(ShellPort::Get()->GetDisplayInfo(display_id()).id() !=
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
  if (Shell::GetAshConfig() == Config::MASH) {
    ASSERT_TRUE(ShellPort::Get()->GetDisplayInfo(display_id()).id() !=
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

// Test enable smooth screen rotation code path.
TEST_F(ScreenRotationAnimatorTest, RotatesToDifferentRotationWithCopyCallback) {
  // TODO(wutao): needs GetDisplayInfo http://crbug.com/622480.
  if (Shell::GetAshConfig() == Config::MASH) {
    ASSERT_TRUE(ShellPort::Get()->GetDisplayInfo(display_id()).id() !=
                display_id());
    return;
  }

  SetDisplayRotation(display_id(), display::Display::ROTATE_0);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshEnableSmoothScreenRotation);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  WaitForCopyCallback();
  EXPECT_TRUE(test_api()->HasActiveAnimations());

  test_api()->CompleteAnimations();
  EXPECT_FALSE(test_api()->HasActiveAnimations());
}

// If the external display is removed, it should not crash.
TEST_F(ScreenRotationAnimatorTest, RemoveSecondaryDisplayAfterCopyCallback) {
  // TODO(wutao): needs GetDisplayInfo http://crbug.com/622480.
  if (Shell::GetAshConfig() == Config::MASH) {
    ASSERT_TRUE(ShellPort::Get()->GetDisplayInfo(display_id()).id() !=
                display_id());
    return;
  }

  UpdateDisplay("640x480,800x600");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  const unsigned int primary_display_id =
      display_manager()->GetDisplayAt(0).id();
  SetScreenRotationAnimator(
      display_manager()->GetDisplayAt(1).id(),
      base::Bind(&ScreenRotationAnimatorTest::RemoveSecondaryDisplay,
                 base::Unretained(this), "640x480"));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshEnableSmoothScreenRotation);
  SetDisplayRotation(display_manager()->GetDisplayAt(1).id(),
                     display::Display::ROTATE_0);
  animator()->Rotate(display::Display::ROTATE_90,
                     display::Display::RotationSource::ROTATION_SOURCE_USER);
  WaitForCopyCallback();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(primary_display_id, display_manager()->GetDisplayAt(0).id());
}

}  // namespace ash

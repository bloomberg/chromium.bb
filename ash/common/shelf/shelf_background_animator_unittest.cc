// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_background_animator.h"

#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/common/shelf/shelf_background_animator_observer.h"
#include "ash/common/shelf/shelf_constants.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {
namespace {

const int kMaxAlpha = 255;

// A valid alpha value that is distinct from any final animation state values.
// Used to check if alpha values are changed during animations.
const int kDummyAlpha = 111;

// Observer that caches alpha values for the last observation.
class TestShelfBackgroundObserver : public ShelfBackgroundAnimatorObserver {
 public:
  TestShelfBackgroundObserver() {}
  ~TestShelfBackgroundObserver() override {}

  int background_alpha() const { return background_alpha_; }

  int item_background_alpha() const { return item_background_alpha_; }

  // ShelfBackgroundObserver:
  void UpdateShelfBackground(int alpha) override;
  void UpdateShelfItemBackground(int alpha) override;

 private:
  int background_alpha_ = 0;
  int item_background_alpha_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfBackgroundObserver);
};

void TestShelfBackgroundObserver::UpdateShelfBackground(int alpha) {
  background_alpha_ = alpha;
}

void TestShelfBackgroundObserver::UpdateShelfItemBackground(int alpha) {
  item_background_alpha_ = alpha;
}

}  // namespace

// Provides internal access to a ShelfBackgroundAnimator instance.
class ShelfBackgroundAnimatorTestApi {
 public:
  explicit ShelfBackgroundAnimatorTestApi(ShelfBackgroundAnimator* animator)
      : animator_(animator) {}

  ~ShelfBackgroundAnimatorTestApi() {}

  ShelfBackgroundType previous_background_type() const {
    return animator_->previous_background_type_;
  }

  gfx::SlideAnimation* animator() const { return animator_->animator_.get(); }

 private:
  // The instance to provide internal access to.
  ShelfBackgroundAnimator* animator_;

  DISALLOW_COPY_AND_ASSIGN(ShelfBackgroundAnimatorTestApi);
};

class ShelfBackgroundAnimatorTest : public testing::Test {
 public:
  ShelfBackgroundAnimatorTest() {}
  ~ShelfBackgroundAnimatorTest() override {}

  // testing::Test:
  void SetUp() override;

 protected:
  // Convenience wrapper for |animator_|'s PaintBackground().
  void PaintBackground(
      ShelfBackgroundType background_type,
      AnimationChangeType change_type = AnimationChangeType::IMMEDIATE);

  // Set all of the alpha values for the |observer_|.
  void SetAlphaValuesOnObserver(int alpha);

  // Completes all the animations.
  void CompleteAnimations();

  TestShelfBackgroundObserver observer_;

  // Test target.
  std::unique_ptr<ShelfBackgroundAnimator> animator_;

  // Provides internal access to |animator_|.
  std::unique_ptr<ShelfBackgroundAnimatorTestApi> test_api_;

  // Used to control the animations.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

 private:
  std::unique_ptr<base::ThreadTaskRunnerHandle> task_runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(ShelfBackgroundAnimatorTest);
};

void ShelfBackgroundAnimatorTest::SetUp() {
  task_runner_ = new base::TestMockTimeTaskRunner();
  task_runner_handle_.reset(new base::ThreadTaskRunnerHandle(task_runner_));

  animator_.reset(
      new ShelfBackgroundAnimator(SHELF_BACKGROUND_DEFAULT, nullptr));
  animator_->AddObserver(&observer_);

  test_api_.reset(new ShelfBackgroundAnimatorTestApi(animator_.get()));
}

void ShelfBackgroundAnimatorTest::PaintBackground(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  animator_->PaintBackground(background_type, change_type);
}

void ShelfBackgroundAnimatorTest::SetAlphaValuesOnObserver(int alpha) {
  observer_.UpdateShelfBackground(alpha);
  observer_.UpdateShelfItemBackground(alpha);
}

void ShelfBackgroundAnimatorTest::CompleteAnimations() {
  task_runner_->FastForwardUntilNoTasksRemain();
}

// Verify the |previous_background_type_| and |target_background_type_| values
// when animating to the same target type multiple times.
TEST_F(ShelfBackgroundAnimatorTest, BackgroundTypesWhenAnimatingToSameTarget) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, animator_->target_background_type());

  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, animator_->target_background_type());
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, test_api_->previous_background_type());

  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, animator_->target_background_type());
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, test_api_->previous_background_type());
}

// Verify subsequent calls to PaintBackground() using the
// AnimationChangeType::ANIMATE change type are ignored.
TEST_F(ShelfBackgroundAnimatorTest,
       MultipleAnimateCallsToSameTargetAreIgnored) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);
  SetAlphaValuesOnObserver(kDummyAlpha);
  animator_->PaintBackground(SHELF_BACKGROUND_DEFAULT,
                             AnimationChangeType::ANIMATE);
  CompleteAnimations();

  EXPECT_NE(observer_.background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.item_background_alpha(), kDummyAlpha);

  SetAlphaValuesOnObserver(kDummyAlpha);
  animator_->PaintBackground(SHELF_BACKGROUND_DEFAULT,
                             AnimationChangeType::ANIMATE);
  CompleteAnimations();

  EXPECT_EQ(observer_.background_alpha(), kDummyAlpha);
  EXPECT_EQ(observer_.item_background_alpha(), kDummyAlpha);
}

// Verify observers are updated with the current values when they are added.
TEST_F(ShelfBackgroundAnimatorTest, ObserversUpdatedWhenAdded) {
  animator_->RemoveObserver(&observer_);
  SetAlphaValuesOnObserver(kDummyAlpha);

  animator_->AddObserver(&observer_);

  EXPECT_NE(observer_.background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.item_background_alpha(), kDummyAlpha);
}

// Verify the alpha values for the SHELF_BACKGROUND_DEFAULT state.
TEST_F(ShelfBackgroundAnimatorTest, DefaultBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, animator_->target_background_type());
  EXPECT_EQ(0, observer_.background_alpha());
  EXPECT_EQ(kShelfTranslucentAlpha, observer_.item_background_alpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_OVERLAP state.
TEST_F(ShelfBackgroundAnimatorTest, OverlapBackground) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP);

  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, animator_->target_background_type());
  EXPECT_EQ(kShelfTranslucentAlpha, observer_.background_alpha());
  EXPECT_EQ(0, observer_.item_background_alpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_MAXIMIZED state.
TEST_F(ShelfBackgroundAnimatorTest, MaximizedBackground) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);

  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, animator_->target_background_type());
  EXPECT_EQ(kMaxAlpha, observer_.background_alpha());
  EXPECT_EQ(0, observer_.item_background_alpha());
}

TEST_F(ShelfBackgroundAnimatorTest,
       AnimatorIsDetroyedWhenCompletingSuccessfully) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP, AnimationChangeType::ANIMATE);
  EXPECT_TRUE(test_api_->animator());
  CompleteAnimations();
  EXPECT_FALSE(test_api_->animator());
}

TEST_F(ShelfBackgroundAnimatorTest,
       AnimatorDestroyedWhenChangingBackgroundImmediately) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP, AnimationChangeType::ANIMATE);
  EXPECT_TRUE(test_api_->animator());

  PaintBackground(SHELF_BACKGROUND_OVERLAP, AnimationChangeType::IMMEDIATE);
  EXPECT_FALSE(test_api_->animator());
}

// Verify that existing animator is used when animating to the previous state.
TEST_F(ShelfBackgroundAnimatorTest,
       ExistingAnimatorIsReusedWhenAnimatingToPreviousState) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT, AnimationChangeType::ANIMATE);
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED, AnimationChangeType::ANIMATE);

  const gfx::SlideAnimation* animator = test_api_->animator();
  EXPECT_TRUE(animator);

  PaintBackground(SHELF_BACKGROUND_DEFAULT, AnimationChangeType::ANIMATE);

  EXPECT_EQ(animator, test_api_->animator());
}

// Verify that existing animator is not re-used when the target background isn't
// the same as the previous background.
TEST_F(ShelfBackgroundAnimatorTest,
       ExistingAnimatorNotReusedWhenTargetBackgroundNotPreviousBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT, AnimationChangeType::ANIMATE);
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED, AnimationChangeType::ANIMATE);

  const gfx::SlideAnimation* animator = test_api_->animator();
  EXPECT_TRUE(animator);

  EXPECT_NE(SHELF_BACKGROUND_OVERLAP, test_api_->previous_background_type());
  PaintBackground(SHELF_BACKGROUND_OVERLAP, AnimationChangeType::ANIMATE);

  EXPECT_NE(animator, test_api_->animator());
}

TEST_F(ShelfBackgroundAnimatorTest,
       AnimationProgressesToTargetWhenStoppingUnfinishedAnimator) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP, AnimationChangeType::ANIMATE);

  EXPECT_NE(kShelfTranslucentAlpha, observer_.background_alpha());
  EXPECT_NE(0, observer_.item_background_alpha());

  test_api_->animator()->Stop();

  EXPECT_EQ(kShelfTranslucentAlpha, observer_.background_alpha());
  EXPECT_EQ(0, observer_.item_background_alpha());
}

// Verify observers are always notified, even when alpha values don't change.
TEST_F(ShelfBackgroundAnimatorTest,
       ObserversAreNotifiedWhenSnappingToSameTargetBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  SetAlphaValuesOnObserver(kDummyAlpha);
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_NE(observer_.background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.item_background_alpha(), kDummyAlpha);
}

}  // namespace ash

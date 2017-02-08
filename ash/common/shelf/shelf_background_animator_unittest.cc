// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_background_animator.h"

#include <memory>

#include "ash/common/shelf/shelf_background_animator_observer.h"
#include "ash/common/shelf/shelf_constants.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  int opaque_background_alpha() const { return opaque_background_alpha_; }

  int item_background_alpha() const { return item_background_alpha_; }

  // ShelfBackgroundObserver:
  void UpdateShelfOpaqueBackground(int alpha) override;
  void UpdateShelfItemBackground(int alpha) override;

 private:
  int opaque_background_alpha_ = 0;
  int item_background_alpha_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfBackgroundObserver);
};

void TestShelfBackgroundObserver::UpdateShelfOpaqueBackground(int alpha) {
  opaque_background_alpha_ = alpha;
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

  BackgroundAnimator* opaque_background_animator() const {
    return animator_->opaque_background_animator_.get();
  }

  BackgroundAnimator* item_background_animator() const {
    return animator_->item_background_animator_.get();
  }

  bool can_reuse_animators() const { return animator_->can_reuse_animators_; }

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
  // Convenience wrapper for |animator_|'s PaintBackground() that always uses
  // BACKGROUND_CHANGE_IMMEDIATE.
  void PaintBackground(ShelfBackgroundType background_type);

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
    ShelfBackgroundType background_type) {
  animator_->PaintBackground(background_type, BACKGROUND_CHANGE_IMMEDIATE);
}

void ShelfBackgroundAnimatorTest::SetAlphaValuesOnObserver(int alpha) {
  observer_.UpdateShelfOpaqueBackground(alpha);
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
// BACKGROUND_CHANGE_ANIMATE change type are ignored.
TEST_F(ShelfBackgroundAnimatorTest,
       MultipleAnimateCallsToSameTargetAreIgnored) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);
  SetAlphaValuesOnObserver(kDummyAlpha);
  animator_->PaintBackground(SHELF_BACKGROUND_DEFAULT,
                             BACKGROUND_CHANGE_ANIMATE);
  CompleteAnimations();

  EXPECT_NE(observer_.opaque_background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.item_background_alpha(), kDummyAlpha);

  SetAlphaValuesOnObserver(kDummyAlpha);
  animator_->PaintBackground(SHELF_BACKGROUND_DEFAULT,
                             BACKGROUND_CHANGE_ANIMATE);
  CompleteAnimations();

  EXPECT_EQ(observer_.opaque_background_alpha(), kDummyAlpha);
  EXPECT_EQ(observer_.item_background_alpha(), kDummyAlpha);
}

// Verify observers are updated with the current values when they are added.
TEST_F(ShelfBackgroundAnimatorTest, ObserversUpdatedWhenAdded) {
  animator_->RemoveObserver(&observer_);
  SetAlphaValuesOnObserver(kDummyAlpha);

  animator_->AddObserver(&observer_);

  EXPECT_NE(observer_.opaque_background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.item_background_alpha(), kDummyAlpha);
}

// Verify the alpha values for the SHELF_BACKGROUND_DEFAULT state.
TEST_F(ShelfBackgroundAnimatorTest, DefaultBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, animator_->target_background_type());
  EXPECT_EQ(0, observer_.opaque_background_alpha());
  EXPECT_EQ(kShelfTranslucentAlpha, observer_.item_background_alpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_OVERLAP state.
TEST_F(ShelfBackgroundAnimatorTest, OverlapBackground) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP);

  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, animator_->target_background_type());
  EXPECT_EQ(kShelfTranslucentAlpha, observer_.opaque_background_alpha());
  EXPECT_EQ(0, observer_.item_background_alpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_MAXIMIZED state.
TEST_F(ShelfBackgroundAnimatorTest, MaximizedBackground) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);

  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, animator_->target_background_type());
  EXPECT_EQ(kMaxAlpha, observer_.opaque_background_alpha());
  EXPECT_EQ(0, observer_.item_background_alpha());
}

// Verify that existing animators are used when animating to the previous state.
TEST_F(ShelfBackgroundAnimatorTest, ExistingAnimatorsAreReused) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);
  EXPECT_TRUE(test_api_->can_reuse_animators());

  const BackgroundAnimator* opaque_animator =
      test_api_->opaque_background_animator();
  const BackgroundAnimator* item_animator =
      test_api_->item_background_animator();

  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_EQ(opaque_animator, test_api_->opaque_background_animator());
  EXPECT_EQ(item_animator, test_api_->item_background_animator());
}

// Verify that existing animators are not re-used when the previous animation
// didn't complete.
TEST_F(ShelfBackgroundAnimatorTest,
       ExistingAnimatorsNotReusedWhenPreviousAnimationsDontComplete) {
  EXPECT_NE(SHELF_BACKGROUND_OVERLAP, test_api_->previous_background_type());
  animator_->PaintBackground(SHELF_BACKGROUND_OVERLAP,
                             BACKGROUND_CHANGE_ANIMATE);
  animator_->PaintBackground(SHELF_BACKGROUND_MAXIMIZED,
                             BACKGROUND_CHANGE_ANIMATE);
  EXPECT_FALSE(test_api_->can_reuse_animators());

  const BackgroundAnimator* opaque_animator =
      test_api_->opaque_background_animator();
  const BackgroundAnimator* item_animator =
      test_api_->item_background_animator();

  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, test_api_->previous_background_type());
  animator_->PaintBackground(SHELF_BACKGROUND_OVERLAP,
                             BACKGROUND_CHANGE_ANIMATE);

  EXPECT_NE(opaque_animator, test_api_->opaque_background_animator());
  EXPECT_NE(item_animator, test_api_->item_background_animator());
}

// Verify that existing animators are not re-used when the target background
// isn't the same as the previous background.
TEST_F(ShelfBackgroundAnimatorTest,
       ExistingAnimatorsNotReusedWhenTargetBackgroundNotPreviousBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);
  EXPECT_TRUE(test_api_->can_reuse_animators());

  const BackgroundAnimator* opaque_animator =
      test_api_->opaque_background_animator();
  const BackgroundAnimator* item_animator =
      test_api_->item_background_animator();

  EXPECT_NE(SHELF_BACKGROUND_OVERLAP, test_api_->previous_background_type());
  PaintBackground(SHELF_BACKGROUND_OVERLAP);

  EXPECT_NE(opaque_animator, test_api_->opaque_background_animator());
  EXPECT_NE(item_animator, test_api_->item_background_animator());
}

// Verify animators are not re-used after snapping to the same background type.
TEST_F(ShelfBackgroundAnimatorTest,
       ExistingAnimatorsNotUsedWhenSnappingToSameTargetBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_FALSE(test_api_->can_reuse_animators());
}

// Verify observers are always notified, even when alpha values don't change.
TEST_F(ShelfBackgroundAnimatorTest,
       ObserversAreNotifiedWhenSnappingToSameTargetBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  SetAlphaValuesOnObserver(kDummyAlpha);
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_NE(observer_.opaque_background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.item_background_alpha(), kDummyAlpha);
}

}  // namespace ash

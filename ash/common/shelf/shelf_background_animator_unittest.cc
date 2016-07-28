// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_background_animator.h"

#include <memory>

#include "ash/common/shelf/shelf_background_animator_observer.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/test/material_design_controller_test_api.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using test::MaterialDesignControllerTestAPI;

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

  int asset_background_alpha() const { return asset_background_alpha_; }

  // ShelfBackgroundObserver:
  void UpdateShelfOpaqueBackground(int alpha) override;
  void UpdateShelfAssetBackground(int alpha) override;
  void UpdateShelfItemBackground(int alpha) override;

 private:
  int opaque_background_alpha_ = 0;
  int item_background_alpha_ = 0;
  int asset_background_alpha_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfBackgroundObserver);
};

void TestShelfBackgroundObserver::UpdateShelfOpaqueBackground(int alpha) {
  opaque_background_alpha_ = alpha;
}

void TestShelfBackgroundObserver::UpdateShelfAssetBackground(int alpha) {
  asset_background_alpha_ = alpha;
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

  BackgroundAnimator* asset_background_animator() const {
    return animator_->asset_background_animator_.get();
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

// Note: this is not a parameterized test (like other MD tests) because the
// behavior is so different and not all tests need to be run for both MD and
// non-MD variants.
class ShelfBackgroundAnimatorTestBase : public testing::Test {
 public:
  ShelfBackgroundAnimatorTestBase() {}
  ~ShelfBackgroundAnimatorTestBase() override {}

  virtual MaterialDesignController::Mode GetMaterialDesignMode() = 0;

  int expected_translucent_alpha() const { return expected_translucent_alpha_; }

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

  std::unique_ptr<MaterialDesignControllerTestAPI> material_mode_test_api_;

  // The expected alpha value for translucent items.  Cannot be a constant
  // because it is different for material design and non-material.
  int expected_translucent_alpha_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ShelfBackgroundAnimatorTestBase);
};

void ShelfBackgroundAnimatorTestBase::SetUp() {
  task_runner_ = new base::TestMockTimeTaskRunner();
  task_runner_handle_.reset(new base::ThreadTaskRunnerHandle(task_runner_));

  material_mode_test_api_.reset(
      new MaterialDesignControllerTestAPI(GetMaterialDesignMode()));
  animator_.reset(
      new ShelfBackgroundAnimator(SHELF_BACKGROUND_DEFAULT, nullptr));
  animator_->AddObserver(&observer_);

  test_api_.reset(new ShelfBackgroundAnimatorTestApi(animator_.get()));

  // Initialized after the Material Design mode because GetShelfConstant()
  // depends on the mode.
  expected_translucent_alpha_ = GetShelfConstant(SHELF_BACKGROUND_ALPHA);
}

void ShelfBackgroundAnimatorTestBase::PaintBackground(
    ShelfBackgroundType background_type) {
  animator_->PaintBackground(background_type, BACKGROUND_CHANGE_IMMEDIATE);
}

void ShelfBackgroundAnimatorTestBase::SetAlphaValuesOnObserver(int alpha) {
  observer_.UpdateShelfOpaqueBackground(alpha);
  observer_.UpdateShelfAssetBackground(alpha);
  observer_.UpdateShelfItemBackground(alpha);
}

void ShelfBackgroundAnimatorTestBase::CompleteAnimations() {
  task_runner_->FastForwardUntilNoTasksRemain();
}

class ShelfBackgroundAnimatorNonMDTest
    : public ShelfBackgroundAnimatorTestBase {
 public:
  ShelfBackgroundAnimatorNonMDTest() {}
  ~ShelfBackgroundAnimatorNonMDTest() override {}

  MaterialDesignController::Mode GetMaterialDesignMode() override {
    return MaterialDesignController::NON_MATERIAL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfBackgroundAnimatorNonMDTest);
};

// Verify the alpha values for the SHELF_BACKGROUND_DEFAULT state.
TEST_F(ShelfBackgroundAnimatorNonMDTest, DefaultBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, animator_->target_background_type());
  EXPECT_EQ(0, observer_.opaque_background_alpha());
  EXPECT_EQ(0, observer_.asset_background_alpha());
  EXPECT_EQ(expected_translucent_alpha(), observer_.item_background_alpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_OVERLAP state.
TEST_F(ShelfBackgroundAnimatorNonMDTest, OverlapBackground) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP);

  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, animator_->target_background_type());
  EXPECT_EQ(0, observer_.opaque_background_alpha());
  EXPECT_EQ(expected_translucent_alpha(), observer_.asset_background_alpha());
  EXPECT_EQ(expected_translucent_alpha(), observer_.item_background_alpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_MAXIMIZED state.
TEST_F(ShelfBackgroundAnimatorNonMDTest, MaximizedBackground) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);

  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, animator_->target_background_type());
  EXPECT_EQ(kMaxAlpha, observer_.opaque_background_alpha());
  EXPECT_EQ(expected_translucent_alpha(), observer_.asset_background_alpha());
  EXPECT_EQ(kMaxAlpha, observer_.item_background_alpha());
}

class ShelfBackgroundAnimatorMDTest : public ShelfBackgroundAnimatorTestBase {
 public:
  ShelfBackgroundAnimatorMDTest() {}
  ~ShelfBackgroundAnimatorMDTest() override {}

  MaterialDesignController::Mode GetMaterialDesignMode() override {
    return MaterialDesignController::MATERIAL_EXPERIMENTAL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfBackgroundAnimatorMDTest);
};

// Verify the |previous_background_type_| and |target_background_type_| values
// when animating to the same target type multiple times.
TEST_F(ShelfBackgroundAnimatorMDTest,
       BackgroundTypesWhenAnimatingToSameTarget) {
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
TEST_F(ShelfBackgroundAnimatorMDTest,
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
TEST_F(ShelfBackgroundAnimatorMDTest, ObserversUpdatedWhenAdded) {
  animator_->RemoveObserver(&observer_);
  SetAlphaValuesOnObserver(kDummyAlpha);

  animator_->AddObserver(&observer_);

  EXPECT_NE(observer_.opaque_background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.asset_background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.item_background_alpha(), kDummyAlpha);
}

// Verify the alpha values for the SHELF_BACKGROUND_DEFAULT state.
TEST_F(ShelfBackgroundAnimatorMDTest, DefaultBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, animator_->target_background_type());
  EXPECT_EQ(0, observer_.opaque_background_alpha());
  EXPECT_EQ(0, observer_.asset_background_alpha());
  EXPECT_EQ(expected_translucent_alpha(), observer_.item_background_alpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_OVERLAP state.
TEST_F(ShelfBackgroundAnimatorMDTest, OverlapBackground) {
  PaintBackground(SHELF_BACKGROUND_OVERLAP);

  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, animator_->target_background_type());
  EXPECT_EQ(expected_translucent_alpha(), observer_.opaque_background_alpha());
  EXPECT_EQ(0, observer_.asset_background_alpha());
  EXPECT_EQ(0, observer_.item_background_alpha());
}

// Verify the alpha values for the SHELF_BACKGROUND_MAXIMIZED state.
TEST_F(ShelfBackgroundAnimatorMDTest, MaximizedBackground) {
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);

  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, animator_->target_background_type());
  EXPECT_EQ(kMaxAlpha, observer_.opaque_background_alpha());
  EXPECT_EQ(0, observer_.asset_background_alpha());
  EXPECT_EQ(0, observer_.item_background_alpha());
}

// Verify that existing animators are used when animating to the previous state.
TEST_F(ShelfBackgroundAnimatorMDTest, ExistingAnimatorsAreReused) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);
  EXPECT_TRUE(test_api_->can_reuse_animators());

  const BackgroundAnimator* opaque_animator =
      test_api_->opaque_background_animator();
  const BackgroundAnimator* asset_animator =
      test_api_->asset_background_animator();
  const BackgroundAnimator* item_animator =
      test_api_->item_background_animator();

  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_EQ(opaque_animator, test_api_->opaque_background_animator());
  EXPECT_EQ(asset_animator, test_api_->asset_background_animator());
  EXPECT_EQ(item_animator, test_api_->item_background_animator());
}

// Verify that existing animators are not re-used when the previous animation
// didn't complete.
TEST_F(ShelfBackgroundAnimatorMDTest,
       ExistingAnimatorsNotReusedWhenPreviousAnimationsDontComplete) {
  EXPECT_NE(SHELF_BACKGROUND_OVERLAP, test_api_->previous_background_type());
  animator_->PaintBackground(SHELF_BACKGROUND_OVERLAP,
                             BACKGROUND_CHANGE_ANIMATE);
  animator_->PaintBackground(SHELF_BACKGROUND_MAXIMIZED,
                             BACKGROUND_CHANGE_ANIMATE);
  EXPECT_FALSE(test_api_->can_reuse_animators());

  const BackgroundAnimator* opaque_animator =
      test_api_->opaque_background_animator();
  const BackgroundAnimator* asset_animator =
      test_api_->asset_background_animator();
  const BackgroundAnimator* item_animator =
      test_api_->item_background_animator();

  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, test_api_->previous_background_type());
  animator_->PaintBackground(SHELF_BACKGROUND_OVERLAP,
                             BACKGROUND_CHANGE_ANIMATE);

  EXPECT_NE(opaque_animator, test_api_->opaque_background_animator());
  EXPECT_NE(asset_animator, test_api_->asset_background_animator());
  EXPECT_NE(item_animator, test_api_->item_background_animator());
}

// Verify that existing animators are not re-used when the target background
// isn't the same as the previous background.
TEST_F(ShelfBackgroundAnimatorMDTest,
       ExistingAnimatorsNotReusedWhenTargetBackgroundNotPreviousBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  PaintBackground(SHELF_BACKGROUND_MAXIMIZED);
  EXPECT_TRUE(test_api_->can_reuse_animators());

  const BackgroundAnimator* opaque_animator =
      test_api_->opaque_background_animator();
  const BackgroundAnimator* asset_animator =
      test_api_->asset_background_animator();
  const BackgroundAnimator* item_animator =
      test_api_->item_background_animator();

  EXPECT_NE(SHELF_BACKGROUND_OVERLAP, test_api_->previous_background_type());
  PaintBackground(SHELF_BACKGROUND_OVERLAP);

  EXPECT_NE(opaque_animator, test_api_->opaque_background_animator());
  EXPECT_NE(asset_animator, test_api_->asset_background_animator());
  EXPECT_NE(item_animator, test_api_->item_background_animator());
}

// Verify animators are not re-used after snapping to the same background type.
TEST_F(ShelfBackgroundAnimatorMDTest,
       ExistingAnimatorsNotUsedWhenSnappingToSameTargetBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_FALSE(test_api_->can_reuse_animators());
}

// Verify observers are always notified, even when alpha values don't change.
TEST_F(ShelfBackgroundAnimatorMDTest,
       ObserversAreNotifiedWhenSnappingToSameTargetBackground) {
  PaintBackground(SHELF_BACKGROUND_DEFAULT);
  SetAlphaValuesOnObserver(kDummyAlpha);
  PaintBackground(SHELF_BACKGROUND_DEFAULT);

  EXPECT_NE(observer_.opaque_background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.asset_background_alpha(), kDummyAlpha);
  EXPECT_NE(observer_.item_background_alpha(), kDummyAlpha);
}

}  // namespace ash

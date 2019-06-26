// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_animator.h"

#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/tabs/tab_animation.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TabClosedDetector {
 public:
  void NotifyTabClosed() { was_closed_ = true; }
  bool was_closed_ = false;
};

}  // namespace

class TabStripAnimatorTest : public testing::Test {
 public:
  TabStripAnimatorTest()
      : env_(base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
             base::test::ScopedTaskEnvironment::NowSource::
                 MAIN_THREAD_MOCK_TIME),
        animator_(
            base::BindRepeating(&TabStripAnimatorTest::OnAnimationProgressed,
                                base::Unretained(this))),
        has_animated_(false) {}

  void OnAnimationProgressed() { has_animated_ = true; }

  float OpennessOf(TabAnimationState state) { return state.openness_; }
  float PinnednessOf(TabAnimationState state) { return state.pinnedness_; }
  float ActivenessOf(TabAnimationState state) { return state.activeness_; }

  base::test::ScopedTaskEnvironment env_;
  TabStripAnimator animator_;
  bool has_animated_;
};

TEST_F(TabStripAnimatorTest, StaticStripIsNotAnimating) {
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kInactive,
                                   TabAnimationState::TabPinnedness::kPinned);
  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_FALSE(has_animated_);
}

TEST_F(TabStripAnimatorTest, InsertTabAnimation) {
  animator_.InsertTabAt(0, base::BindOnce([]() {}),
                        TabAnimationState::TabActiveness::kActive,
                        TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(0.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));

  env_.FastForwardBy(2 * TabAnimation::kAnimationDuration);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_TRUE(has_animated_);
}

TEST_F(TabStripAnimatorTest, ChangeActiveTab) {
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  animator_.InsertTabAtNoAnimation(1, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kInactive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));

  animator_.SetActiveTab(0, 1);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));
}

TEST_F(TabStripAnimatorTest, PinAndUnpinTab) {
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_EQ(0.0f, PinnednessOf(animator_.GetCurrentTabStates()[0]));

  animator_.SetPinnednessNoAnimation(0,
                                     TabAnimationState::TabPinnedness::kPinned);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1.0f, PinnednessOf(animator_.GetCurrentTabStates()[0]));

  animator_.SetPinnednessNoAnimation(
      0, TabAnimationState::TabPinnedness::kUnpinned);

  EXPECT_EQ(0.0f, PinnednessOf(animator_.GetCurrentTabStates()[0]));
}

TEST_F(TabStripAnimatorTest, RemoveTabNoAnimation) {
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  animator_.InsertTabAtNoAnimation(1, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kInactive,
                                   TabAnimationState::TabPinnedness::kUnpinned);

  animator_.RemoveTabNoAnimation(1);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
}

TEST_F(TabStripAnimatorTest, RemoveTabAnimation) {
  TabClosedDetector second_tab;
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  animator_.InsertTabAtNoAnimation(
      1,
      base::BindOnce(&TabClosedDetector::NotifyTabClosed,
                     base::Unretained(&second_tab)),
      TabAnimationState::TabActiveness::kInactive,
      TabAnimationState::TabPinnedness::kUnpinned);

  animator_.RemoveTab(1);

  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[1]));
  EXPECT_FALSE(second_tab.was_closed_);

  env_.FastForwardBy(2 * TabAnimation::kAnimationDuration);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_TRUE(second_tab.was_closed_);
}

TEST_F(TabStripAnimatorTest, CompleteAnimations) {
  animator_.InsertTabAt(0, base::BindOnce([]() {}),
                        TabAnimationState::TabActiveness::kActive,
                        TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(0.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));

  animator_.CompleteAnimations();

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));
}

TEST_F(TabStripAnimatorTest, CancelAnimations) {
  animator_.InsertTabAt(0, base::BindOnce([]() {}),
                        TabAnimationState::TabActiveness::kActive,
                        TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(0.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));

  animator_.CancelAnimations();

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(0.0f, OpennessOf(animator_.GetCurrentTabStates()[0]));
}

TEST_F(TabStripAnimatorTest, CompleteAnimationsRemovesClosedTabs) {
  TabClosedDetector second_tab;
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  animator_.InsertTabAtNoAnimation(
      1,
      base::BindOnce(&TabClosedDetector::NotifyTabClosed,
                     base::Unretained(&second_tab)),
      TabAnimationState::TabActiveness::kInactive,
      TabAnimationState::TabPinnedness::kUnpinned);

  animator_.RemoveTab(1);

  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[1]));
  EXPECT_FALSE(second_tab.was_closed_);

  animator_.CompleteAnimations();

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(1u, animator_.GetCurrentTabStates().size());
  EXPECT_TRUE(second_tab.was_closed_);
}

TEST_F(TabStripAnimatorTest, CancelAnimationsDoesNotRemoveClosedTabs) {
  TabClosedDetector second_tab;
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  animator_.InsertTabAtNoAnimation(
      1,
      base::BindOnce(&TabClosedDetector::NotifyTabClosed,
                     base::Unretained(&second_tab)),
      TabAnimationState::TabActiveness::kInactive,
      TabAnimationState::TabPinnedness::kUnpinned);

  animator_.RemoveTab(1);

  EXPECT_TRUE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, OpennessOf(animator_.GetCurrentTabStates()[1]));
  EXPECT_FALSE(second_tab.was_closed_);

  animator_.CancelAnimations();

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_FALSE(second_tab.was_closed_);
}

TEST_F(TabStripAnimatorTest, MoveTabRight) {
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  animator_.InsertTabAtNoAnimation(1, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kInactive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));

  animator_.MoveTabNoAnimation(0, 1);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));
}

TEST_F(TabStripAnimatorTest, MoveTabLeft) {
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  animator_.InsertTabAtNoAnimation(1, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kInactive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));

  animator_.MoveTabNoAnimation(1, 0);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));
}

TEST_F(TabStripAnimatorTest, MoveTabSamePosition) {
  animator_.InsertTabAtNoAnimation(0, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kActive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  animator_.InsertTabAtNoAnimation(1, base::BindOnce([]() {}),
                                   TabAnimationState::TabActiveness::kInactive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));

  animator_.MoveTabNoAnimation(0, 0);

  EXPECT_FALSE(animator_.IsAnimating());
  EXPECT_EQ(2u, animator_.GetCurrentTabStates().size());
  EXPECT_EQ(1.0f, ActivenessOf(animator_.GetCurrentTabStates()[0]));
  EXPECT_EQ(0.0f, ActivenessOf(animator_.GetCurrentTabStates()[1]));
}

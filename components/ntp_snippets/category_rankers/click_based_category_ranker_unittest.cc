// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"

#include "base/memory/ptr_util.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

class ClickBasedCategoryRankerTest : public testing::Test {
 public:
  ClickBasedCategoryRankerTest()
      : pref_service_(base::MakeUnique<TestingPrefServiceSimple>()),
        unused_remote_category_id_(
            static_cast<int>(KnownCategories::LAST_KNOWN_REMOTE_CATEGORY) + 1) {
    ClickBasedCategoryRanker::RegisterProfilePrefs(pref_service_->registry());

    ranker_ = base::MakeUnique<ClickBasedCategoryRanker>(pref_service_.get());
  }

  int GetUnusedRemoteCategoryID() { return unused_remote_category_id_++; }

  Category GetUnusedRemoteCategory() {
    return Category::FromIDValue(GetUnusedRemoteCategoryID());
  }

  bool CompareCategories(const Category& left, const Category& right) {
    return ranker()->Compare(left, right);
  }

  Category AddUnusedRemoteCategory() {
    Category category = GetUnusedRemoteCategory();
    ranker()->AppendCategoryIfNecessary(category);
    return category;
  }

  void AddUnusedRemoteCategories(int quantity) {
    for (int i = 0; i < quantity; ++i) {
      AddUnusedRemoteCategory();
    }
  }

  void ResetRanker() {
    ranker_ = base::MakeUnique<ClickBasedCategoryRanker>(pref_service_.get());
  }

  void NotifyOnSuggestionOpened(int times, Category category) {
    for (int i = 0; i < times; ++i) {
      ranker()->OnSuggestionOpened(category);
    }
  }

  ClickBasedCategoryRanker* ranker() { return ranker_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  int unused_remote_category_id_;
  std::unique_ptr<ClickBasedCategoryRanker> ranker_;

  DISALLOW_COPY_AND_ASSIGN(ClickBasedCategoryRankerTest);
};

TEST_F(ClickBasedCategoryRankerTest, ShouldSortRemoteCategoriesByWhenAdded) {
  const Category first = GetUnusedRemoteCategory();
  const Category second = GetUnusedRemoteCategory();
  // Categories are added in decreasing id order to test that they are not
  // compared by id.
  ranker()->AppendCategoryIfNecessary(second);
  ranker()->AppendCategoryIfNecessary(first);
  EXPECT_TRUE(CompareCategories(second, first));
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldSortLocalCategoriesBeforeRemote) {
  const Category remote_category = AddUnusedRemoteCategory();
  const Category local_category =
      Category::FromKnownCategory(KnownCategories::BOOKMARKS);
  EXPECT_TRUE(CompareCategories(local_category, remote_category));
  EXPECT_FALSE(CompareCategories(remote_category, local_category));
}

TEST_F(ClickBasedCategoryRankerTest,
       CompareShouldReturnFalseForSameCategories) {
  const Category remote_category = AddUnusedRemoteCategory();
  EXPECT_FALSE(CompareCategories(remote_category, remote_category));

  const Category local_category =
      Category::FromKnownCategory(KnownCategories::BOOKMARKS);
  EXPECT_FALSE(CompareCategories(local_category, local_category));
}

TEST_F(ClickBasedCategoryRankerTest,
       AddingMoreRemoteCategoriesShouldNotChangePreviousOrder) {
  AddUnusedRemoteCategories(3);

  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));
  ASSERT_FALSE(CompareCategories(second, first));

  AddUnusedRemoteCategories(3);

  EXPECT_TRUE(CompareCategories(first, second));
  EXPECT_FALSE(CompareCategories(second, first));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldChangeOrderOfNonTopCategories) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));
  ASSERT_FALSE(CompareCategories(second, first));

  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);

  EXPECT_TRUE(CompareCategories(second, first));
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotChangeOrderRightAfterOrderChange) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  // Two non-top categories are added.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();
  ASSERT_TRUE(CompareCategories(first, second));
  ASSERT_FALSE(CompareCategories(second, first));
  // Their order is changed.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);
  ASSERT_TRUE(CompareCategories(second, first));
  ASSERT_FALSE(CompareCategories(first, second));

  // Click on the lower category.
  NotifyOnSuggestionOpened(/*times=*/1, first);

  // Order should not change.
  EXPECT_TRUE(CompareCategories(second, first));
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotMoveCategoryMoreThanOncePerClick) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  // Non-top categories are added.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();
  Category third = AddUnusedRemoteCategory();

  // Move the third category up.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), third);
  EXPECT_TRUE(CompareCategories(third, second));
  // But only on one position even though the first category has low counts.
  EXPECT_TRUE(CompareCategories(first, third));
  // However, another click must move it further.
  NotifyOnSuggestionOpened(/*times=*/1, third);
  EXPECT_TRUE(CompareCategories(third, first));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotMoveTopCategoryRightAfterThreshold) {
  ASSERT_GE(ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin(), 1);

  // At least one top category is added from the default order.
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  // Try to move the second category up as if the first category was non-top.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);

  // Nothing should change, because the first category is top.
  EXPECT_TRUE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldPersistOrderAndClicksWhenRestarted) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  // Non-top categories are added.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();
  Category third = AddUnusedRemoteCategory();

  // Change the order.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), third);
  ASSERT_TRUE(CompareCategories(third, second));
  ASSERT_TRUE(CompareCategories(first, third));

  // Simulate Chrome restart.
  ResetRanker();

  // The old order must be preserved.
  EXPECT_TRUE(CompareCategories(third, second));

  // Clicks must be preserved as well.
  NotifyOnSuggestionOpened(
      /*times=*/1, third);
  EXPECT_TRUE(CompareCategories(third, first));
}

}  // namespace ntp_snippets

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace ntp_snippets {

namespace {

const char kHistogramMovedUpCategoryNewIndex[] =
    "NewTabPage.ContentSuggestions.MovedUpCategoryNewIndex";

}  // namespace

class ClickBasedCategoryRankerTest : public testing::Test {
 public:
  ClickBasedCategoryRankerTest()
      : pref_service_(base::MakeUnique<TestingPrefServiceSimple>()),
        unused_remote_category_id_(
            static_cast<int>(KnownCategories::LAST_KNOWN_REMOTE_CATEGORY) + 1) {
    ClickBasedCategoryRanker::RegisterProfilePrefs(pref_service_->registry());

    ranker_ = base::MakeUnique<ClickBasedCategoryRanker>(
        pref_service_.get(), base::MakeUnique<base::DefaultClock>());
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

  void ResetRanker(std::unique_ptr<base::Clock> clock) {
    ranker_ = base::MakeUnique<ClickBasedCategoryRanker>(pref_service_.get(),
                                                         std::move(clock));
  }

  void NotifyOnSuggestionOpened(int times, Category category) {
    for (int i = 0; i < times; ++i) {
      ranker()->OnSuggestionOpened(category);
    }
  }

  void NotifyOnCategoryDismissed(Category category) {
    ranker()->OnCategoryDismissed(category);
  }

  void SetDismissedCategoryPenaltyVariationParam(int value) {
    variation_params_manager_.SetVariationParamsWithFeatureAssociations(
        ntp_snippets::kStudyName,
        {{"click_based_category_ranker-dismissed_category_penalty",
          base::IntToString(value)}},
        {kCategoryRanker.name});
  }

  void SetPromotedCategoryVariationParam(int value) {
    variation_params_manager_.SetVariationParamsWithFeatureAssociations(
        ntp_snippets::kStudyName,
        {{"click_based_category_ranker-promoted_category",
          base::IntToString(value)}},
        {kCategoryRanker.name});
  }

  ClickBasedCategoryRanker* ranker() { return ranker_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  int unused_remote_category_id_;
  std::unique_ptr<ClickBasedCategoryRanker> ranker_;
  variations::testing::VariationParamsManager variation_params_manager_;

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
  ResetRanker(base::MakeUnique<base::DefaultClock>());

  // The old order must be preserved.
  EXPECT_TRUE(CompareCategories(third, second));

  // Clicks must be preserved as well.
  NotifyOnSuggestionOpened(/*times=*/1, third);
  EXPECT_TRUE(CompareCategories(third, first));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldDecayClickCountsWithTime) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  // Non-top categories are added.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  const int first_clicks = 10 * ClickBasedCategoryRanker::GetPassingMargin();

  // Simulate the user using the first category for a long time (and not using
  // anything else).
  NotifyOnSuggestionOpened(/*times=*/first_clicks, first);

  // Let multiple years pass by.
  auto test_clock = base::MakeUnique<base::SimpleTestClock>();
  base::SimpleTestClock* raw_test_clock = test_clock.get();
  raw_test_clock->SetNow(base::Time::Now() + base::TimeDelta::FromDays(1000));
  // Reset the ranker to pick up the new clock.
  ResetRanker(std::move(test_clock));

  // The user behavior changes and they start using the second category instead.
  // According to our requirenments after such a long time it should take less
  // than |first_clicks| for the second category to outperfom the first one.
  int second_clicks = 0;
  while (CompareCategories(first, second) && second_clicks < first_clicks) {
    NotifyOnSuggestionOpened(/*times=*/1, second);
    second_clicks++;
  }
  EXPECT_THAT(second_clicks, testing::Lt(first_clicks));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldDecayAfterClearHistory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  // The user clears entire history.
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  // Check whether decay happens by clicking on the first category and
  // waiting.
  const int first_clicks = 10 * ClickBasedCategoryRanker::GetPassingMargin();
  NotifyOnSuggestionOpened(/*times=*/first_clicks, first);

  // Let multiple years pass by.
  auto test_clock = base::MakeUnique<base::SimpleTestClock>();
  base::SimpleTestClock* raw_test_clock = test_clock.get();
  raw_test_clock->SetNow(base::Time::Now() + base::TimeDelta::FromDays(1000));
  // Reset the ranker to pick up the new clock.
  ResetRanker(std::move(test_clock));

  // It should take less than |first_clicks| for the second category to
  // overtake because of decays.
  int second_clicks = 0;
  while (CompareCategories(first, second) && second_clicks < first_clicks) {
    NotifyOnSuggestionOpened(/*times=*/1, second);
    second_clicks++;
  }
  EXPECT_THAT(second_clicks, testing::Lt(first_clicks));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldRemoveLastDecayTimeOnClearHistory) {
  ASSERT_NE(ranker()->GetLastDecayTime(), base::Time::FromInternalValue(0));

  // The user clears entire history.
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  EXPECT_EQ(ranker()->GetLastDecayTime(), base::Time::FromInternalValue(0));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldPersistLastDecayTimeWhenRestarted) {
  base::Time before = ranker()->GetLastDecayTime();
  ASSERT_NE(before, base::Time::FromInternalValue(0));

  // Ensure that |Now()| is different from |before| by injecting our clock.
  auto test_clock = base::MakeUnique<base::SimpleTestClock>();
  test_clock->SetNow(base::Time::Now() + base::TimeDelta::FromSeconds(10));
  ResetRanker(std::move(test_clock));

  EXPECT_EQ(before, ranker()->GetLastDecayTime());
}

TEST_F(ClickBasedCategoryRankerTest, ShouldMoveCategoryDownWhenDismissed) {
  SetDismissedCategoryPenaltyVariationParam(2);

  // Take top categories.
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));
  NotifyOnCategoryDismissed(first);
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldMoveSecondToLastCategoryDownWhenDismissed) {
  SetDismissedCategoryPenaltyVariationParam(2);

  // Add categories to the bottom.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));
  NotifyOnCategoryDismissed(first);
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotMoveCategoryTooMuchDownWhenDismissed) {
  SetDismissedCategoryPenaltyVariationParam(2);

  // Add enough categories to the end.
  std::vector<Category> categories;
  const int penalty = ClickBasedCategoryRanker::GetDismissedCategoryPenalty();
  for (int i = 0; i < 2 * penalty + 10; ++i) {
    categories.push_back(AddUnusedRemoteCategory());
  }

  const int target = penalty + 1;
  Category target_category = categories[target];
  for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
    ASSERT_EQ(i < target, CompareCategories(categories[i], target_category));
  }

  // This should move exactly |penalty| categories up.
  NotifyOnCategoryDismissed(categories[target]);

  // Reflect expected change in |categories|.
  const int expected = target + penalty;
  for (int i = target; i + 1 <= expected; ++i) {
    std::swap(categories[i], categories[i + 1]);
  }

  for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
    EXPECT_EQ(i < expected, CompareCategories(categories[i], target_category));
  }
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotChangeOrderOfOtherCategoriesWhenDismissed) {
  SetDismissedCategoryPenaltyVariationParam(2);

  // Add enough categories to the end.
  std::vector<Category> categories;
  const int penalty = ClickBasedCategoryRanker::GetDismissedCategoryPenalty();
  for (int i = 0; i < 2 * penalty + 10; ++i) {
    categories.push_back(AddUnusedRemoteCategory());
  }

  int target = penalty + 1;
  // This should not change order of all other categories.
  NotifyOnCategoryDismissed(categories[target]);

  categories.erase(categories.begin() + target);
  for (int first = 0; first < static_cast<int>(categories.size()); ++first) {
    for (int second = 0; second < static_cast<int>(categories.size());
         ++second) {
      EXPECT_EQ(first < second,
                CompareCategories(categories[first], categories[second]));
    }
  }
}

TEST_F(ClickBasedCategoryRankerTest, ShouldNotMoveLastCategoryWhenDismissed) {
  SetDismissedCategoryPenaltyVariationParam(2);

  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));
  NotifyOnCategoryDismissed(second);
  EXPECT_TRUE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldReduceLastCategoryClicksWhenDismissed) {
  SetDismissedCategoryPenaltyVariationParam(2);

  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));

  NotifyOnSuggestionOpened(/*times=*/1, second);

  // This should reduce the click count back to 0.
  NotifyOnCategoryDismissed(second);

  // Try to move the second category up assuming that the previous click is
  // still there.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin() - 1, second);

  EXPECT_TRUE(CompareCategories(first, second));

  NotifyOnSuggestionOpened(/*times=*/1, second);
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldTakeVariationValueForDismissedCategoryPenalty) {
  const int penalty = 10203;
  SetDismissedCategoryPenaltyVariationParam(penalty);
  EXPECT_EQ(penalty, ClickBasedCategoryRanker::GetDismissedCategoryPenalty());
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldDoNothingWhenCategoryDismissedIfPenaltyIsZero) {
  SetDismissedCategoryPenaltyVariationParam(0);

  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();
  Category third = AddUnusedRemoteCategory();

  NotifyOnSuggestionOpened(/*times=*/1, second);

  // This should be ignored, because the penalty is set to 0.
  NotifyOnCategoryDismissed(second);

  // The second category should stay where it was.
  EXPECT_TRUE(CompareCategories(first, second));
  EXPECT_TRUE(CompareCategories(second, third));

  // Try to move the second category up assuming that the previous click is
  // still there.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin() - 1, second);

  // It should overtake the first category, because the dismissal should be
  // ignored and the click should remain.
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldRestoreDefaultOrderOnClearHistory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  // Change the order.
  while (CompareCategories(first, second)) {
    NotifyOnSuggestionOpened(
        /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);
  }

  ASSERT_FALSE(CompareCategories(first, second));

  // The user clears history.
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  // The default order must be restored.
  EXPECT_TRUE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldPreserveRemoteCategoriesOnClearHistory) {
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));

  // The user clears history.
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  // The order does not matter, but the ranker should not die.
  CompareCategories(first, second);
}

TEST_F(ClickBasedCategoryRankerTest, ShouldIgnorePartialClearHistory) {
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));

  // Change the order.
  while (CompareCategories(first, second)) {
    NotifyOnSuggestionOpened(
        /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);
  }

  ASSERT_FALSE(CompareCategories(first, second));

  // The user partially clears history.
  base::Time begin = base::Time::Now() - base::TimeDelta::FromHours(1),
             end = base::Time::Max();
  ranker()->ClearHistory(begin, end);

  // The order should not be cleared.
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldPromoteCategory) {
  const Category downloads =
      Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  const Category bookmarks =
      Category::FromKnownCategory(KnownCategories::BOOKMARKS);
  const Category articles =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  ASSERT_TRUE(CompareCategories(downloads, bookmarks));
  ASSERT_TRUE(CompareCategories(bookmarks, articles));
  SetPromotedCategoryVariationParam(articles.id());
  ResetRanker(base::MakeUnique<base::DefaultClock>());
  EXPECT_TRUE(CompareCategories(articles, downloads));
  EXPECT_TRUE(CompareCategories(articles, bookmarks));
  EXPECT_FALSE(CompareCategories(downloads, articles));
  EXPECT_FALSE(CompareCategories(bookmarks, articles));
  EXPECT_FALSE(CompareCategories(articles, articles));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldHandleInvalidCategoryIDForPromotion) {
  SetPromotedCategoryVariationParam(
      static_cast<int>(KnownCategories::LOCAL_CATEGORIES_COUNT));
  ResetRanker(base::MakeUnique<base::DefaultClock>());
  // Make sure we have the default order.
  EXPECT_TRUE(CompareCategories(
      Category::FromKnownCategory(KnownCategories::PHYSICAL_WEB_PAGES),
      Category::FromKnownCategory(KnownCategories::DOWNLOADS)));
  EXPECT_TRUE(CompareCategories(
      Category::FromKnownCategory(KnownCategories::DOWNLOADS),
      Category::FromKnownCategory(KnownCategories::RECENT_TABS)));
  EXPECT_TRUE(CompareCategories(
      Category::FromKnownCategory(KnownCategories::RECENT_TABS),
      Category::FromKnownCategory(KnownCategories::FOREIGN_TABS)));
  EXPECT_TRUE(CompareCategories(
      Category::FromKnownCategory(KnownCategories::FOREIGN_TABS),
      Category::FromKnownCategory(KnownCategories::BOOKMARKS)));
  EXPECT_TRUE(CompareCategories(
      Category::FromKnownCategory(KnownCategories::BOOKMARKS),
      Category::FromKnownCategory(KnownCategories::ARTICLES)));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldEndPromotionOnSectionDismissal) {
  const Category physical_web =
      Category::FromKnownCategory(KnownCategories::PHYSICAL_WEB_PAGES);
  const Category articles =
      Category::FromKnownCategory(KnownCategories::ARTICLES);
  ASSERT_TRUE(CompareCategories(physical_web, articles));

  SetPromotedCategoryVariationParam(articles.id());
  ResetRanker(base::MakeUnique<base::DefaultClock>());

  ASSERT_TRUE(CompareCategories(articles, physical_web));

  ranker()->OnCategoryDismissed(articles);
  EXPECT_FALSE(CompareCategories(articles, physical_web));
  EXPECT_TRUE(CompareCategories(physical_web, articles));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldResumePromotionAfter2WeeksSinceDismissal) {
  const Category downloads =
      Category::FromKnownCategory(KnownCategories::DOWNLOADS);
  const Category recent_tabs =
      Category::FromKnownCategory(KnownCategories::RECENT_TABS);
  ASSERT_TRUE(CompareCategories(downloads, recent_tabs));

  SetPromotedCategoryVariationParam(recent_tabs.id());
  ResetRanker(base::MakeUnique<base::DefaultClock>());
  ASSERT_TRUE(CompareCategories(recent_tabs, downloads));

  ranker()->OnCategoryDismissed(recent_tabs);
  ASSERT_FALSE(CompareCategories(recent_tabs, downloads));

  // Simulate a little over 2 weeks of time passing.
  auto test_clock = base::MakeUnique<base::SimpleTestClock>();
  test_clock->SetNow(base::Time::Now() + base::TimeDelta::FromDays(15));
  ResetRanker(std::move(test_clock));
  EXPECT_TRUE(CompareCategories(recent_tabs, downloads));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldEmitNewIndexWhenCategoryMovedUpDueToClick) {
  base::HistogramTester histogram_tester;

  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  // Increase the score of |second| until the order changes.
  while (CompareCategories(first, second)) {
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
        IsEmpty());
    ranker()->OnSuggestionOpened(second);
  }
  ASSERT_FALSE(CompareCategories(first, second));
  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotEmitNewIndexWhenCategoryDismissed) {
  base::HistogramTester histogram_tester;

  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category category = Category::FromKnownCategory(default_order[0]);

  ASSERT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());

  NotifyOnCategoryDismissed(category);

  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotEmitNewIndexOfMovedUpCategoryWhenHistoryCleared) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  // Increase the score of |second| until the order changes.
  while (CompareCategories(first, second)) {
    ranker()->OnSuggestionOpened(second);
  }
  ASSERT_FALSE(CompareCategories(first, second));

  // The histogram tester is created here to ignore previous events.
  base::HistogramTester histogram_tester;
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  // ClearHistory should restore the default order.
  ASSERT_TRUE(CompareCategories(first, second));

  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotEmitNewIndexWhenCategoryPromoted) {
  base::HistogramTester histogram_tester;

  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  ASSERT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());

  SetPromotedCategoryVariationParam(second.id());
  ResetRanker(base::MakeUnique<base::DefaultClock>());

  ASSERT_FALSE(CompareCategories(first, second));

  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());
}

}  // namespace ntp_snippets

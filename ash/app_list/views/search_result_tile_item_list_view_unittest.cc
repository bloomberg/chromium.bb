// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_tile_item_list_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/test/test_search_result.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {

namespace {
constexpr int kMaxNumSearchResultTiles = 6;
constexpr int kInstalledApps = 4;
constexpr int kPlayStoreApps = 2;
constexpr int kRecommendedApps = 1;
}  // namespace

class SearchResultTileItemListViewTest
    : public views::ViewsTestBase,
      public ::testing::WithParamInterface<std::pair<bool, bool>> {
 public:
  SearchResultTileItemListViewTest() = default;
  ~SearchResultTileItemListViewTest() override = default;

 protected:
  void CreateSearchResultTileItemListView() {
    std::vector<base::Feature> enabled_features, disabled_features;
    // Enable fullscreen app list for parameterized Play Store app search
    // feature.
    // Zero State affects the UI behavior significantly. This test tests the
    // UI behavior with zero state being disable.
    // TODO(crbug.com/925195): Write new test cases for zero state.
    if (IsPlayStoreAppSearchEnabled()) {
      enabled_features.push_back(app_list_features::kEnablePlayStoreAppSearch);
    } else {
      disabled_features.push_back(app_list_features::kEnablePlayStoreAppSearch);
    }
    if (IsReinstallAppRecommendationEnabled()) {
      enabled_features.push_back(
          app_list_features::kEnableAppReinstallZeroState);
    } else {
      disabled_features.push_back(
          app_list_features::kEnableAppReinstallZeroState);
    }

    disabled_features.push_back(app_list_features::kEnableZeroStateSuggestions);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    ASSERT_EQ(IsPlayStoreAppSearchEnabled(),
              app_list_features::IsPlayStoreAppSearchEnabled());

    ASSERT_EQ(IsReinstallAppRecommendationEnabled(),
              app_list_features::IsAppReinstallZeroStateEnabled());

    // Sets up the views.
    textfield_ = std::make_unique<views::Textfield>();
    view_ = std::make_unique<SearchResultTileItemListView>(
        nullptr, textfield_.get(), &view_delegate_);
    view_->SetResults(view_delegate_.GetSearchModel()->results());
  }

  bool IsPlayStoreAppSearchEnabled() const { return GetParam().first; }

  bool IsReinstallAppRecommendationEnabled() const { return GetParam().second; }

  SearchResultTileItemListView* view() { return view_.get(); }

  SearchModel::SearchResults* GetResults() {
    return view_delegate_.GetSearchModel()->results();
  }

  void SetUpSearchResults() {
    SearchModel::SearchResults* results = GetResults();

    // Populate results for installed applications.
    for (int i = 0; i < kInstalledApps; ++i) {
      std::unique_ptr<TestSearchResult> result =
          std::make_unique<TestSearchResult>();
      result->set_result_id(base::StringPrintf("InstalledApp %d", i));
      result->set_display_type(ash::SearchResultDisplayType::kTile);
      result->set_result_type(ash::SearchResultType::kInstalledApp);
      result->set_title(
          base::UTF8ToUTF16(base::StringPrintf("InstalledApp %d", i)));
      results->Add(std::move(result));
    }

    // Populate results for Play Store search applications.
    if (IsPlayStoreAppSearchEnabled()) {
      for (int i = 0; i < kPlayStoreApps; ++i) {
        std::unique_ptr<TestSearchResult> result =
            std::make_unique<TestSearchResult>();
        result->set_result_id(base::StringPrintf("PlayStoreApp %d", i));
        result->set_display_type(ash::SearchResultDisplayType::kTile);
        result->set_result_type(ash::SearchResultType::kPlayStoreApp);
        result->set_title(
            base::UTF8ToUTF16(base::StringPrintf("PlayStoreApp %d", i)));
        result->SetRating(1 + i);
        result->SetFormattedPrice(
            base::UTF8ToUTF16(base::StringPrintf("Price %d", i)));
        results->Add(std::move(result));
      }
    }

    if (IsReinstallAppRecommendationEnabled()) {
      for (int i = 0; i < kRecommendedApps; ++i) {
        std::unique_ptr<TestSearchResult> result =
            std::make_unique<TestSearchResult>();
        result->set_result_id(base::StringPrintf("RecommendedApp %d", i));
        result->set_display_type(ash::SearchResultDisplayType::kRecommendation);
        result->set_result_type(ash::SearchResultType::kPlayStoreReinstallApp);
        result->set_title(
            base::UTF8ToUTF16(base::StringPrintf("RecommendedApp %d", i)));
        result->SetRating(1 + i);
        results->Add(std::move(result));
      }
    }

    // Adding results calls SearchResultContainerView::ScheduleUpdate().
    // It will post a delayed task to update the results and relayout.
    RunPendingMessages();
  }

  int GetOpenResultCount(int ranking) {
    int result = view_delegate_.open_search_result_counts()[ranking];
    return result;
  }

  void ResetOpenResultCount() {
    view_delegate_.open_search_result_counts().clear();
  }

  int GetResultCount() const { return view_->num_results(); }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    return view_->OnKeyPressed(event);
  }

 private:
  test::AppListTestViewDelegate view_delegate_;
  std::unique_ptr<SearchResultTileItemListView> view_;
  std::unique_ptr<views::Textfield> textfield_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemListViewTest);
};

TEST_P(SearchResultTileItemListViewTest, Basic) {
  CreateSearchResultTileItemListView();
  SetUpSearchResults();

  const int results = GetResultCount();
  int expected_results = kInstalledApps;

  if (IsPlayStoreAppSearchEnabled()) {
    expected_results += kPlayStoreApps;
  }
  if (IsReinstallAppRecommendationEnabled()) {
    expected_results += kRecommendedApps;
  }
  expected_results = std::min(kMaxNumSearchResultTiles, expected_results);

  EXPECT_EQ(expected_results, results);

  const bool separators_enabled =
      IsPlayStoreAppSearchEnabled() || IsReinstallAppRecommendationEnabled();
  // When the Play Store app search feature or app reinstallation feature is
  // enabled, for each result, we added a separator for result type grouping.
  const int expected_child_count = separators_enabled
                                       ? kMaxNumSearchResultTiles * 2
                                       : kMaxNumSearchResultTiles;
  EXPECT_EQ(expected_child_count, view()->child_count());

  /// Test accessibility descriptions of tile views.
  const int first_child = separators_enabled ? 1 : 0;
  const int child_step = separators_enabled ? 2 : 1;

  for (int i = 0; i < kInstalledApps; ++i) {
    ui::AXNodeData node_data;
    view()
        ->child_at(first_child + i * child_step)
        ->GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ax::mojom::Role::kButton, node_data.role);
    EXPECT_EQ(base::StringPrintf("InstalledApp %d", i),
              node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  const int expected_install_apps =
      expected_results -
      (IsReinstallAppRecommendationEnabled() ? kRecommendedApps : 0) -
      kInstalledApps;
  for (int i = kInstalledApps; i < (kInstalledApps + expected_install_apps);
       ++i) {
    ui::AXNodeData node_data;
    view()
        ->child_at(first_child + i * child_step)
        ->GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ax::mojom::Role::kButton, node_data.role);
    EXPECT_EQ(base::StringPrintf("PlayStoreApp %d, Star rating %d.0, Price %d",
                                 i - kInstalledApps, i + 1 - kInstalledApps,
                                 i - kInstalledApps),
              node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  // Recommendations.
  const int start_index = kInstalledApps + expected_install_apps;
  for (int i = kInstalledApps + expected_install_apps; i < expected_results;
       ++i) {
    ui::AXNodeData node_data;
    view()
        ->child_at(first_child + i * child_step)
        ->GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ax::mojom::Role::kButton, node_data.role);
    EXPECT_EQ(base::StringPrintf(
                  "RecommendedApp %d, Star rating %d.0, App recommendation",
                  i - start_index, i + 1 - start_index),
              node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  ResetOpenResultCount();
  for (int i = 0; i < results; ++i) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
    for (int j = 0; j <= i; ++j)
      view()->tile_views_for_test()[i]->OnKeyEvent(&event);
    // When both app reinstalls and play store apps are enabled, we actually
    // instantiate 7 results, but only show 6. So we have to look, for exactly 1
    // result, a "skip" ahead for the reinstall result.
    if (IsReinstallAppRecommendationEnabled() &&
        IsPlayStoreAppSearchEnabled() && i == (results - 1)) {
      EXPECT_EQ(i + 1, GetOpenResultCount(i + 1));
    } else {
      EXPECT_EQ(i + 1, GetOpenResultCount(i));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SearchResultTileItemListViewTest,
                         testing::ValuesIn({std::make_pair(false, false),
                                            std::make_pair(false, true),
                                            std::make_pair(true, false),
                                            std::make_pair(true, true)}));

}  // namespace app_list

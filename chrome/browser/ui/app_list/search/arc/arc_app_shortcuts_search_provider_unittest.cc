// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcuts_search_provider.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

namespace {
constexpr char kFakeAppPackageName[] = "FakeAppPackageName";
}  // namespace

class ArcAppShortcutsSearchProviderTest
    : public AppListTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  ArcAppShortcutsSearchProviderTest() = default;
  ~ArcAppShortcutsSearchProviderTest() override = default;

  // AppListTestBase:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_test_.SetUp(profile());
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
  }

  void TearDown() override {
    controller_.reset();
    arc_test_.TearDown();
    AppListTestBase::TearDown();
  }

  void CreateRanker(const base::Feature& feature,
                    const std::map<std::string, std::string>& params = {}) {
    if (!params.empty()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(feature, params);
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      ranker_ =
          std::make_unique<AppSearchResultRanker>(temp_dir_.GetPath(),
                                                  /*is_ephemeral_user=*/false);
    } else {
      scoped_feature_list_.InitWithFeatures({}, {feature});
    }
  }

  arc::mojom::AppInfo CreateAppInfo(const std::string& name,
                                    const std::string& activity,
                                    const std::string& package_name) {
    arc::mojom::AppInfo appinfo;
    appinfo.name = name;
    appinfo.package_name = package_name;
    appinfo.activity = activity;
    return appinfo;
  }

  std::string AddArcAppAndShortcut(const arc::mojom::AppInfo& app_info,
                                   bool launchable) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    // Adding app to the prefs, and check that the app is accessible by id.
    prefs->AddAppAndShortcut(
        app_info.name, app_info.package_name, app_info.activity,
        std::string() /* intent_uri */, std::string() /* icon_resource_id */,
        false /* sticky */, true /* notifications_enabled */,
        true /* app_ready */, false /* suspended */, false /* shortcut */,
        launchable);
    const std::string app_id =
        ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
    EXPECT_TRUE(prefs->GetApp(app_id));
    return app_id;
  }

  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AppSearchResultRanker> ranker_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppShortcutsSearchProviderTest);
};

TEST_P(ArcAppShortcutsSearchProviderTest, Basic) {
  CreateRanker(app_list_features::kEnableZeroStateAppsRanker);
  EXPECT_EQ(ranker_, nullptr);
  const bool launchable = GetParam();

  const std::string app_id = AddArcAppAndShortcut(
      CreateAppInfo("FakeName", "FakeActivity", kFakeAppPackageName),
      launchable);

  const size_t kMaxResults = launchable ? 4 : 0;
  constexpr char kQuery[] = "shortlabel";

  auto provider = std::make_unique<ArcAppShortcutsSearchProvider>(
      kMaxResults, profile(), controller_.get(), ranker_.get());
  EXPECT_TRUE(provider->results().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  provider->Start(base::UTF8ToUTF16(kQuery));
  const auto& results = provider->results();
  EXPECT_EQ(kMaxResults, results.size());
  // Verify search results.
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %zu", i),
              base::UTF16ToUTF8(results[i]->title()));
    EXPECT_EQ(ash::SearchResultDisplayType::kTile, results[i]->display_type());
  }
}

TEST_F(ArcAppShortcutsSearchProviderTest, RankerIsDisableWithFlag) {
  CreateRanker(app_list_features::kEnableQueryBasedAppsRanker);
  EXPECT_EQ(ranker_, nullptr);

  const std::string app_id = AddArcAppAndShortcut(
      CreateAppInfo("FakeName", "FakeActivity", kFakeAppPackageName), true);
  const size_t kMaxResults = 4;
  constexpr char kQuery[] = "shortlabel";
  constexpr char kPrefix[] = "appshortcutsearch://";
  constexpr char kShortcutId[] = "/ShortcutId ";

  // Create a search provider and train with kMaxResults shortcuts.
  auto provider = std::make_unique<ArcAppShortcutsSearchProvider>(
      kMaxResults, profile(), controller_.get(), ranker_.get());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  for (size_t i = 0; i < kMaxResults; i++) {
    provider->Train(
        base::StrCat({kPrefix, app_id, kShortcutId, base::NumberToString(i)}),
        RankingItemType::kArcAppShortcut);
  }
  provider->Start(base::UTF8ToUTF16(kQuery));

  // Currently, without ranker, relevance scores for app
  // shortcuts are always 0.
  const auto& results = provider->results();
  for (const auto& result : results)
    EXPECT_EQ(result->relevance(), 0);
}

TEST_F(ArcAppShortcutsSearchProviderTest, RankerImproveScores) {
  CreateRanker(app_list_features::kEnableQueryBasedAppsRanker,
               {{"rank_arc_app_shortcuts", "true"}, {"max_items_to_get", "6"}});
  EXPECT_NE(ranker_, nullptr);

  const std::string app_id = AddArcAppAndShortcut(
      CreateAppInfo("FakeName", "FakeActivity", kFakeAppPackageName), true);
  const size_t kMaxResults = 6;
  constexpr char kQuery[] = "shortlabel";
  constexpr char kPrefix[] = "appshortcutsearch://";
  constexpr char kShortcutId[] = "/ShortcutId ";

  // Create a search provider and train with kMaxResults shortcuts.
  auto provider = std::make_unique<ArcAppShortcutsSearchProvider>(
      kMaxResults, profile(), controller_.get(), ranker_.get());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  for (size_t i = 0; i < kMaxResults; i++) {
    provider->Train(
        base::StrCat({kPrefix, app_id, kShortcutId, base::NumberToString(i)}),
        RankingItemType::kArcAppShortcut);
  }
  provider->Start(base::UTF8ToUTF16(kQuery));
  // Verify search results to see whether they were increased.
  const auto& results = provider->results();
  for (const auto& result : results)
    EXPECT_GT(result->relevance(), 0);
}

TEST_F(ArcAppShortcutsSearchProviderTest, EmptyQueries) {
  CreateRanker(app_list_features::kEnableZeroStateAppsRanker,
               {{"rank_arc_app_shortcuts", "true"}, {"max_items_to_get", "6"}});
  EXPECT_NE(ranker_, nullptr);

  const std::string app_id = AddArcAppAndShortcut(
      CreateAppInfo("FakeName", "FakeActivity", kFakeAppPackageName), true);
  const size_t kMaxResults = 6;
  constexpr char kQuery[] = "";
  constexpr char kPrefix[] = "appshortcutsearch://";
  constexpr char kShortcutId[] = "/ShortcutId ";

  // Create a search provider
  auto provider = std::make_unique<ArcAppShortcutsSearchProvider>(
      kMaxResults, profile(), controller_.get(), ranker_.get());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  for (size_t i = 0; i < kMaxResults; i++) {
    provider->Train(
        base::StrCat({kPrefix, app_id, kShortcutId, base::NumberToString(i)}),
        RankingItemType::kArcAppShortcut);
  }
  provider->Start(base::UTF8ToUTF16(kQuery));
  // Verify search results.
  const auto& results = provider->results();
  EXPECT_EQ(results.size(), kMaxResults);
  for (const auto& result : results) {
    EXPECT_EQ(ash::SearchResultDisplayType::kRecommendation,
              result->display_type());
    EXPECT_GT(result->relevance(), 0);
  }
}

INSTANTIATE_TEST_SUITE_P(, ArcAppShortcutsSearchProviderTest, testing::Bool());

}  // namespace app_list

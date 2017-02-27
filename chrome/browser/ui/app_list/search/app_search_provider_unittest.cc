// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_search_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_result.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "components/arc/test/fake_app_instance.h"
#endif

namespace app_list {
namespace test {

const base::Time kTestCurrentTime = base::Time::FromInternalValue(100000);

bool MoreRelevant(const SearchResult* result1, const SearchResult* result2) {
  return result1->relevance() > result2->relevance();
}

class AppSearchProviderTest : public AppListTestBase {
 public:
  AppSearchProviderTest() {}
  ~AppSearchProviderTest() override {}

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    model_.reset(new app_list::AppListModel);
    controller_.reset(new ::test::TestAppListControllerDelegate);
    builder_.reset(new ExtensionAppModelBuilder(controller_.get()));
    builder_->InitializeWithProfile(profile_.get(), model_.get());
  }

  void CreateSearch() {
    std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock());
    clock->SetNow(kTestCurrentTime);
    app_search_.reset(new AppSearchProvider(profile_.get(), nullptr,
                                            std::move(clock),
                                            model_->top_level_item_list()));
  }

  std::string RunQuery(const std::string& query) {
    app_search_->Start(false, base::UTF8ToUTF16(query));

    // Sort results by relevance.
    std::vector<SearchResult*> sorted_results;
    for (const auto& result : app_search_->results())
      sorted_results.emplace_back(result.get());
    std::sort(sorted_results.begin(), sorted_results.end(), &MoreRelevant);

    std::string result_str;
    for (size_t i = 0; i < sorted_results.size(); ++i) {
      if (!result_str.empty())
        result_str += ',';

      result_str += base::UTF16ToUTF8(sorted_results[i]->title());
    }
    return result_str;
  }

  AppListModel* model() { return model_.get(); }
  const SearchProvider::Results& results() { return app_search_->results(); }

 private:
  std::unique_ptr<app_list::AppListModel> model_;
  std::unique_ptr<AppSearchProvider> app_search_;
  std::unique_ptr<ExtensionAppModelBuilder> builder_;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProviderTest);
};

TEST_F(AppSearchProviderTest, Basic) {
#if defined(OS_CHROMEOS)
  ArcAppTest arc_test;
  arc_test.SetUp(profile());
  arc_test.app_instance()->RefreshAppList();
  std::vector<arc::mojom::AppInfo> arc_apps(arc_test.fake_apps().begin(),
                                            arc_test.fake_apps().begin() + 2);
  arc_test.app_instance()->SendRefreshAppList(arc_apps);
#endif

  CreateSearch();

  EXPECT_EQ("", RunQuery("!@#$-,-_"));
  EXPECT_EQ("", RunQuery("unmatched query"));

  // Search for "pa" should return both packaged app. The order is undefined
  // because the test only considers textual relevance and the two apps end
  // up having the same score.
  std::string result = RunQuery("pa");
  EXPECT_TRUE(result == "Packaged App 1,Packaged App 2" ||
              result == "Packaged App 2,Packaged App 1");

  EXPECT_EQ("Packaged App 1", RunQuery("pa1"));
  EXPECT_EQ("Packaged App 2", RunQuery("pa2"));
  EXPECT_EQ("Packaged App 1", RunQuery("papp1"));
  EXPECT_EQ("Hosted App", RunQuery("host"));

#if defined(OS_CHROMEOS)
  result = RunQuery("fake");
  EXPECT_TRUE(result == "Fake App 0,Fake App 1" ||
              result == "Fake App 1,Fake App 0");
  result = RunQuery("app1");
  EXPECT_TRUE(result == "Packaged App 1,Fake App 1" ||
              result == "Fake App 1,Packaged App 1");
#endif
}

TEST_F(AppSearchProviderTest, DisableAndEnable) {
  CreateSearch();

  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->DisableExtension(kHostedAppId,
                             extensions::Extension::DISABLE_NONE);
  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->EnableExtension(kHostedAppId);
  EXPECT_EQ("Hosted App", RunQuery("host"));
}

TEST_F(AppSearchProviderTest, UninstallExtension) {
  CreateSearch();

  EXPECT_EQ("Packaged App 1", RunQuery("pa1"));
  EXPECT_FALSE(results().empty());
  service_->UninstallExtension(kPackagedApp1Id,
                               extensions::UNINSTALL_REASON_FOR_TESTING,
                               base::Bind(&base::DoNothing),
                               NULL);

  // Allow async AppSearchProvider::UpdateResults to run.
  base::RunLoop().RunUntilIdle();

  // Uninstalling an app should update the result list without needing to start
  // a new search.
  EXPECT_TRUE(results().empty());

  // Rerunning the query also should return no results.
  EXPECT_EQ("", RunQuery("pa1"));

  // Let uninstall code to clean up.
  base::RunLoop().RunUntilIdle();
}

#if defined(OS_CHROMEOS)
TEST_F(AppSearchProviderTest, InstallUninstallArc) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());
  std::vector<arc::mojom::AppInfo> arc_apps;
  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(arc_apps);

  CreateSearch();

  EXPECT_TRUE(results().empty());
  EXPECT_EQ("", RunQuery("fapp0"));

  arc_apps.push_back(arc_test.fake_apps()[0]);
  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(arc_apps);

  // Allow async AppSearchProvider::UpdateResults to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("Fake App 0", RunQuery("fapp0"));
  EXPECT_FALSE(results().empty());

  arc_apps.clear();
  arc_test.app_instance()->RefreshAppList();
  arc_test.app_instance()->SendRefreshAppList(arc_apps);

  // Allow async AppSearchProvider::UpdateResults to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(results().empty());
  EXPECT_EQ("", RunQuery("fapp0"));

  // Let uninstall code to clean up.
  base::RunLoop().RunUntilIdle();
}
#endif

TEST_F(AppSearchProviderTest, FetchRecommendations) {
  CreateSearch();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  prefs->SetLastLaunchTime(kHostedAppId, base::Time::FromInternalValue(20));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(5));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));

  prefs->SetLastLaunchTime(kHostedAppId, base::Time::FromInternalValue(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(20));
  EXPECT_EQ("Packaged App 2,Packaged App 1,Hosted App", RunQuery(""));

  // Times in the future should just be handled as highest priority.
  prefs->SetLastLaunchTime(kHostedAppId,
                           kTestCurrentTime + base::TimeDelta::FromSeconds(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(5));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));
}

TEST_F(AppSearchProviderTest, FetchUnlaunchedRecommendations) {
  CreateSearch();

  const char kFolderId[] = "folder1";
  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  // The order of unlaunched recommendations should be based on the install time
  // order.
  prefs->SetLastLaunchTime(kHostedAppId, base::Time::Now());
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(0));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(0));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));

  // Switching the app list order should not change the query result.
  model()->SetItemPosition(
      model()->FindItem(kPackagedApp2Id),
      model()->FindItem(kPackagedApp1Id)->position().CreateBefore());
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));

  // Moving an app into a folder should not deprioritize it.
  model()->AddItem(std::unique_ptr<AppListFolderItem>(
      new AppListFolderItem(kFolderId, AppListFolderItem::FOLDER_TYPE_NORMAL)));
  model()->MoveItemToFolder(model()->FindItem(kPackagedApp1Id), kFolderId);
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));

  // The position of the folder shouldn't matter.
  model()->SetItemPosition(
      model()->FindItem(kFolderId),
      model()->FindItem(kPackagedApp2Id)->position().CreateBefore());
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));
}

}  // namespace test
}  // namespace app_list

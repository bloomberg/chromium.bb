// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/app_list/search/app_search_provider.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_set.h"
#include "sync/api/string_ordinal.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_result.h"

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

    scoped_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock());
    clock->SetNow(kTestCurrentTime);
    model_.reset(new app_list::AppListModel);
    controller_.reset(new ::test::TestAppListControllerDelegate);
    builder_.reset(new ExtensionAppModelBuilder(controller_.get()));
    builder_->InitializeWithProfile(profile_.get(), model_.get());
    app_search_.reset(new AppSearchProvider(
        profile_.get(), nullptr, clock.Pass(), model_->top_level_item_list()));
  }

  std::string RunQuery(const std::string& query) {
    app_search_->Start(false, base::UTF8ToUTF16(query));

    // Sort results by relevance.
    std::vector<SearchResult*> sorted_results;
    std::copy(app_search_->results().begin(),
              app_search_->results().end(),
              std::back_inserter(sorted_results));
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
  scoped_ptr<app_list::AppListModel> model_;
  scoped_ptr<AppSearchProvider> app_search_;
  scoped_ptr<ExtensionAppModelBuilder> builder_;
  scoped_ptr<::test::TestAppListControllerDelegate> controller_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProviderTest);
};

TEST_F(AppSearchProviderTest, Basic) {
  EXPECT_EQ("", RunQuery("!@#$-,-_"));
  EXPECT_EQ("", RunQuery("unmatched query"));

  // Search for "pa" should return both packaged app. The order is undefined
  // because the test only considers textual relevance and the two apps end
  // up having the same score.
  const std::string result = RunQuery("pa");
  EXPECT_TRUE(result == "Packaged App 1,Packaged App 2" ||
              result == "Packaged App 2,Packaged App 1");

  EXPECT_EQ("Packaged App 1", RunQuery("pa1"));
  EXPECT_EQ("Packaged App 2", RunQuery("pa2"));
  EXPECT_EQ("Packaged App 1", RunQuery("app1"));
  EXPECT_EQ("Hosted App", RunQuery("host"));
}

TEST_F(AppSearchProviderTest, DisableAndEnable) {
  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->DisableExtension(kHostedAppId,
                             extensions::Extension::DISABLE_NONE);
  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->EnableExtension(kHostedAppId);
  EXPECT_EQ("Hosted App", RunQuery("host"));
}

TEST_F(AppSearchProviderTest, Uninstall) {
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

TEST_F(AppSearchProviderTest, FetchRecommendations) {
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
  const char kFolderId[] = "folder1";
  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  // The order of unlaunched recommendations should be based on the app list
  // order and be sorted below launched items.
  prefs->SetLastLaunchTime(kHostedAppId,
                           kTestCurrentTime - base::TimeDelta::FromSeconds(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, base::Time::FromInternalValue(0));
  prefs->SetLastLaunchTime(kPackagedApp2Id, base::Time::FromInternalValue(0));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));

  // Switching the app list order should change the query result.
  model()->SetItemPosition(
      model()->FindItem(kPackagedApp2Id),
      model()->FindItem(kPackagedApp1Id)->position().CreateBefore());
  EXPECT_EQ("Hosted App,Packaged App 2,Packaged App 1", RunQuery(""));

  // Moving an app into a folder deprioritizes it.
  model()->AddItem(scoped_ptr<AppListFolderItem>(
      new AppListFolderItem(kFolderId, AppListFolderItem::FOLDER_TYPE_NORMAL)));
  model()->MoveItemToFolder(model()->FindItem(kPackagedApp2Id), kFolderId);
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));

  // The position of the folder shouldn't matter.
  model()->SetItemPosition(
      model()->FindItem(kFolderId),
      model()->FindItem(kPackagedApp1Id)->position().CreateBefore());
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunQuery(""));
}

}  // namespace test
}  // namespace app_list

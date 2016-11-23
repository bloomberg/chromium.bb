// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile_statistics_aggregator.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildBookmarkModelWithoutLoad(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model(
      new bookmarks::BookmarkModel(base::MakeUnique<ChromeBookmarkClient>(
          profile, ManagedBookmarkServiceFactory::GetForProfile(profile))));
  return std::move(bookmark_model);
}

void LoadBookmarkModel(Profile* profile,
                       bookmarks::BookmarkModel* bookmark_model) {
  bookmark_model->Load(profile->GetPrefs(), profile->GetPath(),
                       profile->GetIOTaskRunner(),
                       content::BrowserThread::GetTaskRunnerForThread(
                           content::BrowserThread::UI));
}

bookmarks::BookmarkModel* CreateBookmarkModelWithoutLoad(Profile* profile) {
  return static_cast<bookmarks::BookmarkModel*>(
      BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, BuildBookmarkModelWithoutLoad));
}

class BookmarkStatHelper {
 public:
  BookmarkStatHelper() : num_of_times_called_(0) {}

  void StatsCallback(profiles::ProfileCategoryStats stats) {
    if (stats.back().category == profiles::kProfileStatisticsBookmarks)
      ++num_of_times_called_;
  }

  int GetNumOfTimesCalled() { return num_of_times_called_; }

 private:
  base::Closure quit_closure_;
  int num_of_times_called_;
};

void VerifyStatisticsCache(const base::FilePath& profile_path,
    const std::map<std::string, int>& expected,
    const std::vector<std::string>& categories_to_check) {
  const profiles::ProfileCategoryStats actual =
      ProfileStatistics::GetProfileStatisticsFromAttributesStorage(
          profile_path);

  EXPECT_EQ(categories_to_check.size(), actual.size());

  std::set<std::string> checked;
  for (const auto& stat : actual) {
    bool has_category = expected.count(stat.category);
    EXPECT_EQ(has_category, stat.success);
    EXPECT_EQ(has_category ? expected.at(stat.category) : 0, stat.count);
    EXPECT_TRUE(checked.insert(stat.category).second);
  }
}
}  // namespace

class ProfileStatisticsTest : public testing::Test {
 public:
  ProfileStatisticsTest() : manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ProfileStatisticsTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(manager_.SetUp());
  }

  void TearDown() override {
  }

  TestingProfileManager* manager() { return &manager_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager manager_;
};

TEST_F(ProfileStatisticsTest, ProfileAttributesStorage) {
  TestingProfile* profile = manager()->CreateTestingProfile("Test 1");
  ASSERT_TRUE(profile);
  base::FilePath profile_path = profile->GetPath();

  std::vector<std::string> categories_to_check;
  categories_to_check.push_back(profiles::kProfileStatisticsBrowsingHistory);
  categories_to_check.push_back(profiles::kProfileStatisticsPasswords);
  categories_to_check.push_back(profiles::kProfileStatisticsBookmarks);
  categories_to_check.push_back(profiles::kProfileStatisticsSettings);

  std::vector<std::pair<std::string, int>> insertions;
  int num = 3;
  // Insert for the first round, overwrite for the second round.
  for (int i = 0; i < 2; i++) {
    for (const auto& category : categories_to_check)
      insertions.push_back(std::make_pair(category, num++));
  }

  std::map<std::string, int> expected;
  // Now no keys are set.
  VerifyStatisticsCache(profile_path, expected, categories_to_check);
  // Insert items and test after each insert.
  for (const auto& item : insertions) {
    ProfileStatistics::SetProfileStatisticsToAttributesStorage(
        profile_path, item.first, item.second);
    expected[item.first] = item.second;
    VerifyStatisticsCache(profile_path, expected, categories_to_check);
  }
}

TEST_F(ProfileStatisticsTest, WaitOrCountBookmarks) {
  TestingProfile* profile = manager()->CreateTestingProfile("Test 1");
  ASSERT_TRUE(profile);

  bookmarks::BookmarkModel* bookmark_model =
      CreateBookmarkModelWithoutLoad(profile);
  ASSERT_TRUE(bookmark_model);

  // Run ProfileStatisticsAggregator::WaitOrCountBookmarks.
  ProfileStatisticsAggregator* aggregator;
  BookmarkStatHelper bookmark_stat_helper;
  base::RunLoop run_loop_aggregator_destruction;
  // The following should run inside a scope, so the scoped_refptr gets deleted
  // immediately.
  {
    scoped_refptr<ProfileStatisticsAggregator> aggregator_scoped =
        new ProfileStatisticsAggregator(
                profile,
                base::Bind(&BookmarkStatHelper::StatsCallback,
                           base::Unretained(&bookmark_stat_helper)),
                run_loop_aggregator_destruction.QuitClosure());
    aggregator = aggregator_scoped.get();
  }
  // Wait until ProfileStatisticsAggregator::WaitOrCountBookmarks is run.
  base::RunLoop run_loop1;
  run_loop1.RunUntilIdle();
  EXPECT_EQ(0, bookmark_stat_helper.GetNumOfTimesCalled());

  // Run ProfileStatisticsAggregator::WaitOrCountBookmarks again.
  aggregator->AddCallbackAndStartAggregator(
      profiles::ProfileStatisticsCallback());
  // Wait until ProfileStatisticsAggregator::WaitOrCountBookmarks is run.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(0, bookmark_stat_helper.GetNumOfTimesCalled());

  // Load the bookmark model. When the model is loaded (asynchronously), the
  // observer added by WaitOrCountBookmarks is run.
  LoadBookmarkModel(profile, bookmark_model);

  run_loop_aggregator_destruction.Run();
  EXPECT_EQ(1, bookmark_stat_helper.GetNumOfTimesCalled());
}

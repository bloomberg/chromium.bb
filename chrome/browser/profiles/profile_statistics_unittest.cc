// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
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
  TestingProfileManager manager_;
  content::TestBrowserThreadBundle thread_bundle_;
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

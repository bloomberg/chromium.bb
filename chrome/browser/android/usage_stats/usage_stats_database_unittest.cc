// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_database.h"

#include <utility>

#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;

namespace usage_stats {

namespace {

const char kFqdn1[] = "foo.com";
const char kFqdn2[] = "bar.org";

const char kToken1[] = "token1";
const char kToken2[] = "token2";

}  // namespace

class UsageStatsDatabaseTest : public testing::Test {
 public:
  UsageStatsDatabaseTest() {
    auto fake_db = std::make_unique<FakeDB<UsageStat>>(&fake_db_unowned_store_);

    // Maintain a pointer to the FakeDB for test callback execution.
    fake_db_unowned_ = fake_db.get();

    usage_stats_database_ =
        std::make_unique<UsageStatsDatabase>(std::move(fake_db));
  }

  UsageStatsDatabase* usage_stats_database() {
    return usage_stats_database_.get();
  }

  FakeDB<UsageStat>* fake_db() { return fake_db_unowned_; }

  MOCK_METHOD1(OnUpdateDone, void(UsageStatsDatabase::Error));
  MOCK_METHOD2(OnGetAllSuspensionsDone,
               void(UsageStatsDatabase::Error, std::vector<std::string>));
  MOCK_METHOD2(OnGetAllTokenMappingsDone,
               void(UsageStatsDatabase::Error, UsageStatsDatabase::TokenMap));

 private:
  std::map<std::string, UsageStat> fake_db_unowned_store_;
  FakeDB<UsageStat>* fake_db_unowned_;
  std::unique_ptr<UsageStatsDatabase> usage_stats_database_;

  DISALLOW_COPY_AND_ASSIGN(UsageStatsDatabaseTest);
};

TEST_F(UsageStatsDatabaseTest, Initialization) {
  ASSERT_NE(nullptr, usage_stats_database());
  ASSERT_NE(nullptr, fake_db());
}

// Suspension Tests

TEST_F(UsageStatsDatabaseTest, SetSuspensionsSuccess) {
  base::flat_set<std::string> domains({kFqdn1, kFqdn2});

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  usage_stats_database()->SetSuspensions(
      domains, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  fake_db()->UpdateCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetSuspensionsFailure) {
  base::flat_set<std::string> domains({kFqdn1, kFqdn2});

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kUnknownError));

  usage_stats_database()->SetSuspensions(
      domains, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  fake_db()->UpdateCallback(false);
}

TEST_F(UsageStatsDatabaseTest, GetAllSuspensionsSuccess) {
  std::vector<std::string> expected;

  EXPECT_CALL(*this, OnGetAllSuspensionsDone(
                         UsageStatsDatabase::Error::kNoError, expected));

  usage_stats_database()->GetAllSuspensions(
      base::BindOnce(&UsageStatsDatabaseTest::OnGetAllSuspensionsDone,
                     base::Unretained(this)));

  fake_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, GetAllSuspensionsFailure) {
  std::vector<std::string> expected;

  EXPECT_CALL(*this, OnGetAllSuspensionsDone(
                         UsageStatsDatabase::Error::kUnknownError, expected));

  usage_stats_database()->GetAllSuspensions(
      base::BindOnce(&UsageStatsDatabaseTest::OnGetAllSuspensionsDone,
                     base::Unretained(this)));

  fake_db()->LoadCallback(false);
}

TEST_F(UsageStatsDatabaseTest, SetAndGetSuspension) {
  // Insert 1 suspension.
  base::flat_set<std::string> domains({kFqdn1});

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  usage_stats_database()->SetSuspensions(
      domains, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  fake_db()->UpdateCallback(true);

  // Get 1 suspension.
  std::vector<std::string> expected({kFqdn1});

  EXPECT_CALL(*this, OnGetAllSuspensionsDone(
                         UsageStatsDatabase::Error::kNoError, expected));

  usage_stats_database()->GetAllSuspensions(
      base::BindOnce(&UsageStatsDatabaseTest::OnGetAllSuspensionsDone,
                     base::Unretained(this)));

  fake_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetRemoveAndGetSuspension) {
  // Insert 2 suspensions.
  base::flat_set<std::string> domains1({kFqdn1, kFqdn2});

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  usage_stats_database()->SetSuspensions(
      domains1, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                               base::Unretained(this)));

  fake_db()->UpdateCallback(true);

  // Insert 1 suspension, and remove the other.
  base::flat_set<std::string> domains2({kFqdn1});

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  usage_stats_database()->SetSuspensions(
      domains2, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                               base::Unretained(this)));

  fake_db()->UpdateCallback(true);

  // Get 1 suspension.
  std::vector<std::string> expected({kFqdn1});

  EXPECT_CALL(*this, OnGetAllSuspensionsDone(
                         UsageStatsDatabase::Error::kNoError, expected));

  usage_stats_database()->GetAllSuspensions(
      base::BindOnce(&UsageStatsDatabaseTest::OnGetAllSuspensionsDone,
                     base::Unretained(this)));

  fake_db()->LoadCallback(true);
}

// Token Mapping Tests

TEST_F(UsageStatsDatabaseTest, SetTokenMappingsSuccess) {
  UsageStatsDatabase::TokenMap mappings({{kToken1, kFqdn1}, {kToken2, kFqdn2}});

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  usage_stats_database()->SetTokenMappings(
      mappings, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                               base::Unretained(this)));

  fake_db()->UpdateCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetTokenMappingsFailure) {
  UsageStatsDatabase::TokenMap mappings({{kToken1, kFqdn1}, {kToken2, kFqdn2}});

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kUnknownError));

  usage_stats_database()->SetTokenMappings(
      mappings, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                               base::Unretained(this)));

  fake_db()->UpdateCallback(false);
}

TEST_F(UsageStatsDatabaseTest, GetAllTokenMappingsSuccess) {
  UsageStatsDatabase::TokenMap expected;

  EXPECT_CALL(*this, OnGetAllTokenMappingsDone(
                         UsageStatsDatabase::Error::kNoError, expected));

  usage_stats_database()->GetAllTokenMappings(
      base::BindOnce(&UsageStatsDatabaseTest::OnGetAllTokenMappingsDone,
                     base::Unretained(this)));

  fake_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, GetAllTokenMappingsFailure) {
  UsageStatsDatabase::TokenMap expected;

  EXPECT_CALL(*this, OnGetAllTokenMappingsDone(
                         UsageStatsDatabase::Error::kUnknownError, expected));

  usage_stats_database()->GetAllTokenMappings(
      base::BindOnce(&UsageStatsDatabaseTest::OnGetAllTokenMappingsDone,
                     base::Unretained(this)));

  fake_db()->LoadCallback(false);
}

TEST_F(UsageStatsDatabaseTest, SetAndGetTokenMapping) {
  UsageStatsDatabase::TokenMap mapping({{kToken1, kFqdn1}});

  // Insert 1 token mapping.
  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  usage_stats_database()->SetTokenMappings(
      mapping, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                              base::Unretained(this)));

  fake_db()->UpdateCallback(true);

  // Get 1 token mapping.
  EXPECT_CALL(*this, OnGetAllTokenMappingsDone(
                         UsageStatsDatabase::Error::kNoError, mapping));

  usage_stats_database()->GetAllTokenMappings(
      base::BindOnce(&UsageStatsDatabaseTest::OnGetAllTokenMappingsDone,
                     base::Unretained(this)));

  fake_db()->LoadCallback(true);
}

TEST_F(UsageStatsDatabaseTest, SetRemoveAndGetTokenMapping) {
  // Insert 2 token mappings.
  UsageStatsDatabase::TokenMap mappings1(
      {{kToken1, kFqdn1}, {kToken2, kFqdn2}});

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  usage_stats_database()->SetTokenMappings(
      mappings1, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                                base::Unretained(this)));

  fake_db()->UpdateCallback(true);

  // Re-insert 1 token mapping, and remove the other.apping) {
  UsageStatsDatabase::TokenMap mappings2({{kToken1, kFqdn1}});

  EXPECT_CALL(*this, OnUpdateDone(UsageStatsDatabase::Error::kNoError));

  usage_stats_database()->SetTokenMappings(
      mappings2, base::BindOnce(&UsageStatsDatabaseTest::OnUpdateDone,
                                base::Unretained(this)));

  fake_db()->UpdateCallback(true);

  // Get 1 remaining token mapping.
  EXPECT_CALL(*this, OnGetAllTokenMappingsDone(
                         UsageStatsDatabase::Error::kNoError, mappings2));

  usage_stats_database()->GetAllTokenMappings(
      base::BindOnce(&UsageStatsDatabaseTest::OnGetAllTokenMappingsDone,
                     base::Unretained(this)));

  fake_db()->LoadCallback(true);
}
}  // namespace usage_stats

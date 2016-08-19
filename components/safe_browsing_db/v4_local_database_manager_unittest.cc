// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing_db/v4_database.h"
#include "components/safe_browsing_db/v4_local_database_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class FakeV4Database : public V4Database {
 public:
  FakeV4Database(const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
                 std::unique_ptr<StoreMap> store_map,
                 const MatchedHashPrefixMap& matched_hash_prefix_map)
      : V4Database(db_task_runner, std::move(store_map)),
        matched_hash_prefix_map_(matched_hash_prefix_map) {}

  void GetStoresMatchingFullHash(
      const FullHash& full_hash,
      const base::hash_set<UpdateListIdentifier>& stores_to_look,
      MatchedHashPrefixMap* matched_hash_prefix_map) override {
    *matched_hash_prefix_map = matched_hash_prefix_map_;
  }

 private:
  const MatchedHashPrefixMap& matched_hash_prefix_map_;
};

class V4LocalDatabaseManagerTest : public PlatformTest {
 public:
  V4LocalDatabaseManagerTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    DVLOG(1) << "base_dir_: " << base_dir_.path().value();

    v4_local_database_manager_ =
        make_scoped_refptr(new V4LocalDatabaseManager(base_dir_.path()));
    v4_local_database_manager_->SetTaskRunnerForTest(task_runner_);

    SetupLocalDatabaseManager();
  }

  void TearDown() override {
    v4_local_database_manager_->StopOnIOThread(true);

    // Force destruction of the database.
    task_runner_->RunPendingTasks();
    base::RunLoop().RunUntilIdle();

    PlatformTest::TearDown();
  }

  void SetupLocalDatabaseManager() {
    v4_local_database_manager_->StartOnIOThread(NULL, V4ProtocolConfig());

    task_runner_->RunPendingTasks();
    base::RunLoop().RunUntilIdle();
  }

  void ReplaceV4Database(const MatchedHashPrefixMap& matched_hash_prefix_map) {
    v4_local_database_manager_->v4_database_.reset(new FakeV4Database(
        task_runner_, base::MakeUnique<StoreMap>(), matched_hash_prefix_map));
  }

  void ForceDisableLocalDatabaseManager() {
    v4_local_database_manager_->enabled_ = false;
  }

  base::ScopedTempDir base_dir_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<V4LocalDatabaseManager> v4_local_database_manager_;
};

TEST_F(V4LocalDatabaseManagerTest, TestGetThreatSource) {
  EXPECT_EQ(ThreatSource::LOCAL_PVER4,
            v4_local_database_manager_->GetThreatSource());
}

TEST_F(V4LocalDatabaseManagerTest, TestIsSupported) {
  EXPECT_TRUE(v4_local_database_manager_->IsSupported());
}

TEST_F(V4LocalDatabaseManagerTest, TestCanCheckUrl) {
  EXPECT_TRUE(
      v4_local_database_manager_->CanCheckUrl(GURL("http://example.com/a/")));
  EXPECT_TRUE(
      v4_local_database_manager_->CanCheckUrl(GURL("https://example.com/a/")));
  EXPECT_TRUE(
      v4_local_database_manager_->CanCheckUrl(GURL("ftp://example.com/a/")));
  EXPECT_FALSE(
      v4_local_database_manager_->CanCheckUrl(GURL("adp://example.com/a/")));
}

TEST_F(V4LocalDatabaseManagerTest,
       TestCheckBrowseUrlWithEmptyStoresReturnsNoMatch) {
  // Both the stores are empty right now so CheckBrowseUrl should return true.
  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), nullptr));
}

TEST_F(V4LocalDatabaseManagerTest, TestCheckBrowseUrlWithFakeDbReturnsMatch) {
  MatchedHashPrefixMap matched_hash_prefix_map;
  matched_hash_prefix_map[GetUrlMalwareId()] = HashPrefix("aaaa");
  ReplaceV4Database(matched_hash_prefix_map);

  // The fake database returns a matched hash prefix.
  EXPECT_FALSE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), nullptr));
}

TEST_F(V4LocalDatabaseManagerTest,
       TestCheckBrowseUrlReturnsNoMatchWhenDisabled) {
  MatchedHashPrefixMap matched_hash_prefix_map;
  matched_hash_prefix_map[GetUrlMalwareId()] = HashPrefix("aaaa");
  ReplaceV4Database(matched_hash_prefix_map);

  // The same URL returns |false| in the previous test because
  // v4_local_database_manager_ is enabled.
  ForceDisableLocalDatabaseManager();

  EXPECT_TRUE(v4_local_database_manager_->CheckBrowseUrl(
      GURL("http://example.com/a/"), nullptr));
}

}  // namespace safe_browsing

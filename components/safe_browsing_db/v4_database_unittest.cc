// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing_db/v4_database.h"
#include "components/safe_browsing_db/v4_store.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class FakeV4Store : public V4Store {
 public:
  FakeV4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
              const base::FilePath& store_path,
              const bool reset_succeeds)
      : V4Store(
            task_runner,
            base::FilePath(store_path.value() + FILE_PATH_LITERAL(".fake"))),
        reset_succeeds_(reset_succeeds) {}

  bool Reset() override { return reset_succeeds_; }

 private:
  bool reset_succeeds_;
};

// This factory creates a "fake" store. It allows the caller to specify that the
// first store should fail on Reset() i.e. return false. All subsequent stores
// always return true. This is used to test the Reset() method in V4Database.
class FakeV4StoreFactory : public V4StoreFactory {
 public:
  FakeV4StoreFactory(bool next_store_reset_fails)
      : next_store_reset_fails_(next_store_reset_fails) {}

  V4Store* CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path) override {
    bool reset_succeeds = !next_store_reset_fails_;
    next_store_reset_fails_ = false;
    return new FakeV4Store(task_runner, store_path, reset_succeeds);
  }

 private:
  bool next_store_reset_fails_;
};

class SafeBrowsingV4DatabaseTest : public PlatformTest {
 public:
  SafeBrowsingV4DatabaseTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Setup a database in a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_dirname_ =
        temp_dir_.path().AppendASCII("SafeBrowsingV4DatabaseTest");

    created_but_not_called_back_ = false;
    created_and_called_back_ = false;
  }

  void SetupInfoMapAndExpectedState() {
    UpdateListIdentifier update_list_identifier;

    update_list_identifier.platform_type = WINDOWS_PLATFORM;
    update_list_identifier.threat_entry_type = URL;
    update_list_identifier.threat_type = MALWARE_THREAT;
    list_info_map_[update_list_identifier] = "win_url_malware";
    expected_identifiers_.push_back(update_list_identifier);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("win_url_malware.fake"));

    update_list_identifier.platform_type = LINUX_PLATFORM;
    update_list_identifier.threat_entry_type = URL;
    update_list_identifier.threat_type = MALWARE_THREAT;
    list_info_map_[update_list_identifier] = "linux_url_malware";
    expected_identifiers_.push_back(update_list_identifier);
    expected_store_paths_.push_back(
        database_dirname_.AppendASCII("linux_url_malware.fake"));
  }

  void NewDatabaseReadyWithExpectedStorePathsAndIds(
      std::vector<base::FilePath> expected_store_paths,
      std::vector<UpdateListIdentifier> expected_identifiers,
      bool expected_resets_successfully,
      std::unique_ptr<V4Database> v4_database) {
    ASSERT_TRUE(v4_database);
    ASSERT_TRUE(v4_database->store_map_);

    // The following check ensures that the callback was called asynchronously.
    EXPECT_TRUE(created_but_not_called_back_);

    ASSERT_EQ(expected_store_paths.size(), v4_database->store_map_->size());
    ASSERT_EQ(expected_identifiers.size(), v4_database->store_map_->size());
    for (size_t i = 0; i < expected_identifiers.size(); i++) {
      const auto& expected_identifier = expected_identifiers[i];
      const auto& store = (*v4_database->store_map_)[expected_identifier];
      ASSERT_TRUE(store);
      const auto& expected_store_path = expected_store_paths[i];
      EXPECT_EQ(expected_store_path, store->store_path());
    }

    EXPECT_EQ(expected_resets_successfully, v4_database->ResetDatabase());

    EXPECT_FALSE(created_and_called_back_);
    created_and_called_back_ = true;
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<V4Database> v4_database_;
  base::FilePath database_dirname_;
  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle thread_bundle_;
  bool created_but_not_called_back_;
  bool created_and_called_back_;
  ListInfoMap list_info_map_;
  std::vector<UpdateListIdentifier> expected_identifiers_;
  std::vector<base::FilePath> expected_store_paths_;
};

// Test to set up the database with no stores.
TEST_F(SafeBrowsingV4DatabaseTest, TestSetupDatabaseWithNoStores) {
  NewDatabaseReadyCallback callback_db_ready = base::Bind(
      &SafeBrowsingV4DatabaseTest::NewDatabaseReadyWithExpectedStorePathsAndIds,
      base::Unretained(this), expected_store_paths_, expected_identifiers_,
      true);
  V4Database::Create(task_runner_, database_dirname_, list_info_map_,
                     callback_db_ready);
  created_but_not_called_back_ = true;
  task_runner_->RunPendingTasks();

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(true, created_and_called_back_);
}

// Test to set up the database with fake stores.
TEST_F(SafeBrowsingV4DatabaseTest, TestSetupDatabaseWithFakeStores) {
  SetupInfoMapAndExpectedState();

  NewDatabaseReadyCallback callback_db_ready = base::Bind(
      &SafeBrowsingV4DatabaseTest::NewDatabaseReadyWithExpectedStorePathsAndIds,
      base::Unretained(this), expected_store_paths_, expected_identifiers_,
      true);

  FakeV4StoreFactory* factory = new FakeV4StoreFactory(false);
  ANNOTATE_LEAKING_OBJECT_PTR(factory);
  V4Database::RegisterStoreFactoryForTest(factory);
  V4Database::Create(task_runner_, database_dirname_, list_info_map_,
                     callback_db_ready);
  created_but_not_called_back_ = true;
  task_runner_->RunPendingTasks();

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(true, created_and_called_back_);
}

// Test to set up the database with fake stores that fail to reset.
TEST_F(SafeBrowsingV4DatabaseTest, TestSetupDatabaseWithFakeStoresFailsReset) {
  SetupInfoMapAndExpectedState();

  NewDatabaseReadyCallback callback_db_ready = base::Bind(
      &SafeBrowsingV4DatabaseTest::NewDatabaseReadyWithExpectedStorePathsAndIds,
      base::Unretained(this), expected_store_paths_, expected_identifiers_,
      false);

  FakeV4StoreFactory* factory = new FakeV4StoreFactory(true);
  ANNOTATE_LEAKING_OBJECT_PTR(factory);
  V4Database::RegisterStoreFactoryForTest(factory);
  V4Database::Create(task_runner_, database_dirname_, list_info_map_,
                     callback_db_ready);
  created_but_not_called_back_ = true;
  task_runner_->RunPendingTasks();

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(true, created_and_called_back_);
}

}  // namespace safe_browsing

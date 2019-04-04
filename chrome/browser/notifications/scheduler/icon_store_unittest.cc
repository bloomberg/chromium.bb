// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/icon_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/proto/icon.pb.h"
#include "chrome/browser/notifications/scheduler/icon_entry.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

const char kEntryKey[] = "guid_key1";
const char kEntryKey2[] = "guid_key2";
const char kEntryId[] = "proto_id_1";
const char kEntryId2[] = "proto_id_2";
const char kEntryData[] = "data_1";
const char kEntryData2[] = "data_2";

class IconStoreTest : public testing::Test {
 public:
  IconStoreTest() : load_result_(false), db_(nullptr) {}
  ~IconStoreTest() override = default;

  void SetUp() override {
    IconEntry entry;
    entry.uuid = kEntryId;
    entry.data = kEntryData;
    proto::Icon proto;
    leveldb_proto::DataToProto(&entry, &proto);
    db_entries_.emplace(kEntryKey, proto);

    auto db =
        std::make_unique<leveldb_proto::test::FakeDB<proto::Icon, IconEntry>>(
            &db_entries_);
    db_ = db.get();
    store_ = std::make_unique<IconProtoDbStore>(std::move(db));
  }

  void InitDb() {
    store()->Init(base::DoNothing());
    db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  }

  void Load(const std::string& key) {
    store()->Load(key, base::BindOnce(&IconStoreTest::OnEntryLoaded,
                                      base::Unretained(this)));
  }

  void OnEntryLoaded(bool success, std::unique_ptr<IconEntry> entry) {
    loaded_entry_ = std::move(entry);
    load_result_ = success;
  }

 protected:
  IconStore* store() { return store_.get(); }
  leveldb_proto::test::FakeDB<proto::Icon, IconEntry>* db() { return db_; }
  bool load_result() const { return load_result_; }
  IconEntry* loaded_entry() { return loaded_entry_.get(); }

  void VerifyEntry(const std::string& uuid, const std::string& data) {
    DCHECK(loaded_entry_);
    EXPECT_EQ(loaded_entry_->uuid, uuid);
    EXPECT_EQ(loaded_entry_->data, data);
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<IconStore> store_;
  std::map<std::string, proto::Icon> db_entries_;
  std::unique_ptr<IconEntry> loaded_entry_;
  bool load_result_;
  leveldb_proto::test::FakeDB<proto::Icon, IconEntry>* db_;

  DISALLOW_COPY_AND_ASSIGN(IconStoreTest);
};

TEST_F(IconStoreTest, Init) {
  store()->Init(base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  base::RunLoop().RunUntilIdle();
}

TEST_F(IconStoreTest, InitFailed) {
  store()->Init(base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kCorrupt);
  base::RunLoop().RunUntilIdle();
}

TEST_F(IconStoreTest, LoadOne) {
  InitDb();
  Load(kEntryKey);
  db()->GetCallback(true);

  // Verify data is loaded.
  DCHECK(loaded_entry());
  EXPECT_TRUE(load_result());
  VerifyEntry(kEntryId, kEntryData);
}

TEST_F(IconStoreTest, LoadFailed) {
  InitDb();
  Load(kEntryKey);
  db()->GetCallback(false);

  // Verify load failure.
  EXPECT_FALSE(loaded_entry());
  EXPECT_FALSE(load_result());
}

TEST_F(IconStoreTest, Add) {
  InitDb();

  auto new_entry = std::make_unique<IconEntry>();
  new_entry->uuid = kEntryId2;
  new_entry->data = kEntryData;

  store()->Add(kEntryKey2, std::move(new_entry), base::DoNothing());
  db()->UpdateCallback(true);

  // Verify the entry is added.
  Load(kEntryKey2);
  db()->GetCallback(true);
  EXPECT_TRUE(load_result());
  VerifyEntry(kEntryId2, kEntryData);
}

TEST_F(IconStoreTest, AddDuplicate) {
  InitDb();

  auto new_entry = std::make_unique<IconEntry>();
  new_entry->uuid = kEntryId;
  new_entry->data = kEntryData2;

  store()->Add(kEntryKey, std::move(new_entry), base::DoNothing());
  db()->UpdateCallback(true);

  // Add a duplicate id is currently allowed, we just update the entry.
  Load(kEntryKey);
  db()->GetCallback(true);
  EXPECT_TRUE(load_result());
  VerifyEntry(kEntryId, kEntryData2);
}

TEST_F(IconStoreTest, Delete) {
  InitDb();

  // Delete the only entry.
  store()->Delete(kEntryKey,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  // No entry can be loaded, move nullptr as result.
  Load(kEntryKey);
  db()->GetCallback(true);
  EXPECT_FALSE(loaded_entry());
  EXPECT_TRUE(load_result());
}

}  // namespace
}  // namespace notifications

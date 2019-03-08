// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/proto_db_collection_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

// Entry for test which will serialize into leveldb_proto.TestProto.
struct TestEntry {
  bool operator==(const TestEntry& other) const {
    return id == other.id && data == other.data;
  }
  std::string id;
  std::string data;
};

// A implementation of ProtoDbCollectionStore used to test
// ProtoDbCollectionStore<T, P>.
class ProtoStoreForTest
    : public ProtoDbCollectionStore<TestEntry, leveldb_proto::TestProto> {
 public:
  using Entries =
      typename ProtoDbCollectionStore<TestEntry,
                                      leveldb_proto::TestProto>::Entries;
  ProtoStoreForTest(
      std::unique_ptr<leveldb_proto::ProtoDatabase<leveldb_proto::TestProto>>
          db)
      : ProtoDbCollectionStore(std::move(db),
                               std::string("ProtoStoreForTest")) {}
  ~ProtoStoreForTest() override = default;

 private:
  // ProtoDbCollectionStore implementation.
  leveldb_proto::TestProto EntryToProto(const TestEntry& entry) override {
    leveldb_proto::TestProto proto;
    proto.set_id(entry.id);
    proto.set_data(entry.data);
    return proto;
  }

  std::unique_ptr<TestEntry> ProtoToEntry(
      leveldb_proto::TestProto& proto) override {
    auto entry = std::make_unique<TestEntry>();
    entry->id = proto.id();
    entry->data = proto.data();
    return entry;
  }

  DISALLOW_COPY_AND_ASSIGN(ProtoStoreForTest);
};

// Verifies that |entry| and |proto| contains the same data.
void VerifyEntryProto(const TestEntry& entry,
                      const leveldb_proto::TestProto& proto) {
  EXPECT_EQ(entry.id, proto.id());
  EXPECT_EQ(entry.data, proto.data());
}

const char kProtoKey[] = "guid_1234";
const char kProtoId[] = "proto_id";
const char kProtoData[] = "data_1234";

class ProtoDbCollectionStoreTest : public testing::Test {
 public:
  ProtoDbCollectionStoreTest() : db_(nullptr) {}
  ~ProtoDbCollectionStoreTest() override = default;

  void SetUp() override {
    leveldb_proto::TestProto proto;
    proto.set_id(kProtoId);
    proto.set_data(kProtoData);
    db_protos_.emplace(kProtoKey, proto);
    auto db =
        std::make_unique<leveldb_proto::test::FakeDB<leveldb_proto::TestProto>>(
            &db_protos_);
    db_ = db.get();
    store_ = std::make_unique<ProtoStoreForTest>(std::move(db));
  }

  void OnDbInitAndLoad(base::RepeatingClosure quit_closure,
                       bool expected_success,
                       bool success,
                       ProtoStoreForTest::Entries entries) {
    EXPECT_EQ(expected_success, success);
    entries_ = std::move(entries);
    quit_closure.Run();
  }

 protected:
  ProtoStoreForTest* store() { return store_.get(); }
  leveldb_proto::test::FakeDB<leveldb_proto::TestProto>* db() { return db_; }
  const std::map<std::string, leveldb_proto::TestProto>& db_protos() const {
    return db_protos_;
  }
  ProtoStoreForTest::Entries* entries() { return &entries_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<ProtoStoreForTest> store_;
  std::vector<std::unique_ptr<TestEntry>> entries_;
  std::map<std::string, leveldb_proto::TestProto> db_protos_;
  leveldb_proto::test::FakeDB<leveldb_proto::TestProto>* db_;

  DISALLOW_COPY_AND_ASSIGN(ProtoDbCollectionStoreTest);
};

TEST_F(ProtoDbCollectionStoreTest, Init) {
  store()->Init(base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDbCollectionStoreTest, InitFailed) {
  store()->Init(base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kCorrupt);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDbCollectionStoreTest, InitAndLoad) {
  base::RunLoop run_loop;
  store()->InitAndLoad(
      base::BindOnce(&ProtoDbCollectionStoreTest::OnDbInitAndLoad,
                     base::Unretained(this), run_loop.QuitClosure(), true));
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  db()->LoadCallback(true);

  EXPECT_EQ(entries()->size(), 1u);
  VerifyEntryProto(*entries()->front(), db_protos().begin()->second);
}

TEST_F(ProtoDbCollectionStoreTest, InitAndLoadFailedInit) {
  store()->InitAndLoad(
      base::BindOnce([](bool success, ProtoStoreForTest::Entries entries) {
        EXPECT_FALSE(success);
        EXPECT_TRUE(entries.empty());
      }));

  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
}

TEST_F(ProtoDbCollectionStoreTest, InitAndLoadFailedLoad) {
  store()->InitAndLoad(
      base::BindOnce([](bool success, ProtoStoreForTest::Entries entries) {
        EXPECT_FALSE(success);
        EXPECT_TRUE(entries.empty());
      }));

  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  db()->LoadCallback(false);
}

TEST_F(ProtoDbCollectionStoreTest, LoadOne) {
  store()->Init(base::DoNothing());
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  base::RunLoop run_loop;
  store()->Load(
      kProtoKey,
      base::BindOnce(&ProtoDbCollectionStoreTest::OnDbInitAndLoad,
                     base::Unretained(this), run_loop.QuitClosure(), true));
  db()->LoadCallback(true);
  VerifyEntryProto(*entries()->front(), db_protos().begin()->second);
}

TEST_F(ProtoDbCollectionStoreTest, Add) {
  store()->Init(base::DoNothing());
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  // Add a new entry.
  std::string new_key = std::string(kProtoKey) + "_new";
  TestEntry new_entry;
  new_entry.id = "new_id";
  new_entry.data = "new_data";
  store()->Add(new_key, new_entry,
               base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Load the new entry just added.
  db()->UpdateCallback(true);
  store()->Load(new_key, base::BindOnce([](bool success,
                                           ProtoStoreForTest::Entries entries) {
                  EXPECT_TRUE(success);
                  EXPECT_EQ(entries.size(), 1u);
                  EXPECT_EQ(entries.front()->id, "new_id");
                  EXPECT_EQ(entries.front()->data, "new_data");
                }));
  db()->LoadCallback(true);
}

TEST_F(ProtoDbCollectionStoreTest, Delete) {
  store()->InitAndLoad(base::DoNothing());
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  db()->LoadCallback(true);

  // Delete the only entry.
  store()->Delete(kProtoKey,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  // Load the deleted entry.
  store()->Load(
      kProtoKey,
      base::BindOnce([](bool success, ProtoStoreForTest::Entries entries) {
        EXPECT_TRUE(success);
        EXPECT_TRUE(entries.empty());
      }));

  db()->LoadCallback(true);
}

}  // namespace
}  // namespace notifications

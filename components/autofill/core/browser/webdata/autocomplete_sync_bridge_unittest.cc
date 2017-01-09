// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/fake_model_type_change_processor.h"
#include "components/sync/model/metadata_batch.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::AutofillSpecifics;
using sync_pb::EntitySpecifics;
using syncer::EntityDataPtr;
using syncer::EntityData;

namespace autofill {

namespace {

const char kNameFormat[] = "name %d";
const char kValueFormat[] = "value %d";

void VerifyEqual(const AutofillSpecifics& s1, const AutofillSpecifics& s2) {
  EXPECT_EQ(s1.SerializeAsString(), s2.SerializeAsString());
}

void VerifyDataBatch(std::map<std::string, AutofillSpecifics> expected,
                     std::unique_ptr<syncer::DataBatch> batch) {
  while (batch->HasNext()) {
    const syncer::KeyAndData& pair = batch->Next();
    auto iter = expected.find(pair.first);
    ASSERT_NE(iter, expected.end());
    VerifyEqual(iter->second, pair.second->specifics.autofill());
    // Removing allows us to verify we don't see the same item multiple times,
    // and that we saw everything we expected.
    expected.erase(iter);
  }
  EXPECT_TRUE(expected.empty());
}

std::unique_ptr<syncer::ModelTypeChangeProcessor>
CreateModelTypeChangeProcessor(syncer::ModelType type,
                               syncer::ModelTypeSyncBridge* bridge) {
  return base::MakeUnique<syncer::FakeModelTypeChangeProcessor>();
}

AutofillEntry CreateAutofillEntry(
    const sync_pb::AutofillSpecifics& autofill_specifics) {
  AutofillKey key(base::UTF8ToUTF16(autofill_specifics.name()),
                  base::UTF8ToUTF16(autofill_specifics.value()));
  base::Time date_created, date_last_used;
  const google::protobuf::RepeatedField<int64_t>& timestamps =
      autofill_specifics.usage_timestamp();
  if (!timestamps.empty()) {
    date_created = base::Time::FromInternalValue(*timestamps.begin());
    date_last_used = base::Time::FromInternalValue(*timestamps.rbegin());
  }
  return AutofillEntry(key, date_created, date_last_used);
}

// Creates an EntityData/EntityDataPtr around a copy of the given specifics.
EntityDataPtr SpecificsToEntity(const AutofillSpecifics& specifics) {
  EntityData data;
  data.client_tag_hash = "ignored";
  *data.specifics.mutable_autofill() = specifics;
  return data.PassToPtr();
}

class FakeAutofillBackend : public AutofillWebDataBackend {
 public:
  FakeAutofillBackend() {}
  ~FakeAutofillBackend() override {}
  WebDatabase* GetDatabase() override { return db_; }
  void AddObserver(
      autofill::AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveObserver(
      autofill::AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveExpiredFormElements() override {}
  void NotifyOfMultipleAutofillChanges() override {}
  void NotifyThatSyncHasStarted(syncer::ModelType model_type) override {}
  void SetWebDatabase(WebDatabase* db) { db_ = db; }

 private:
  WebDatabase* db_;
};

}  // namespace

class AutocompleteSyncBridgeTest : public testing::Test {
 public:
  AutocompleteSyncBridgeTest() {
    if (temp_dir_.CreateUniqueTempDir()) {
      db_.AddTable(&table_);
      db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
      backend_.SetWebDatabase(&db_);

      bridge_.reset(new AutocompleteSyncBridge(
          &backend_, base::Bind(&CreateModelTypeChangeProcessor)));
    }
  }
  ~AutocompleteSyncBridgeTest() override {}

 protected:
  AutocompleteSyncBridge* bridge() { return bridge_.get(); }

  void SaveSpecificsToTable(
      const std::vector<AutofillSpecifics>& specifics_list) {
    std::vector<AutofillEntry> new_entries;
    for (const auto& specifics : specifics_list) {
      new_entries.push_back(CreateAutofillEntry(specifics));
    }
    table_.UpdateAutofillEntries(new_entries);
  }

  AutofillSpecifics CreateSpecifics(int suffix) {
    AutofillSpecifics specifics;
    specifics.set_name(base::StringPrintf(kNameFormat, suffix));
    specifics.set_value(base::StringPrintf(kValueFormat, suffix));
    specifics.add_usage_timestamp(0);
    return specifics;
  }

  std::string GetStorageKey(const AutofillSpecifics& specifics) {
    std::string key =
        bridge()->GetStorageKey(SpecificsToEntity(specifics).value());
    EXPECT_FALSE(key.empty());
    return key;
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::MessageLoop message_loop_;
  FakeAutofillBackend backend_;
  AutofillTable table_;
  WebDatabase db_;
  std::unique_ptr<AutocompleteSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteSyncBridgeTest);
};

TEST_F(AutocompleteSyncBridgeTest, GetClientTag) {
  // TODO(skym, crbug.com/675991): Implementation.
}

TEST_F(AutocompleteSyncBridgeTest, GetStorageKey) {
  std::string key = GetStorageKey(CreateSpecifics(1));
  EXPECT_EQ(key, GetStorageKey(CreateSpecifics(1)));
  EXPECT_NE(key, GetStorageKey(CreateSpecifics(2)));
}

// Timestamps should not affect storage keys.
TEST_F(AutocompleteSyncBridgeTest, GetStorageKeyTimestamp) {
  AutofillSpecifics specifics = CreateSpecifics(1);
  std::string key = GetStorageKey(specifics);

  specifics.add_usage_timestamp(1);
  EXPECT_EQ(key, GetStorageKey(specifics));

  specifics.add_usage_timestamp(0);
  EXPECT_EQ(key, GetStorageKey(specifics));

  specifics.add_usage_timestamp(-1);
  EXPECT_EQ(key, GetStorageKey(specifics));
}

// Verify that the \0 character is respected as a difference.
TEST_F(AutocompleteSyncBridgeTest, GetStorageKeyNull) {
  AutofillSpecifics specifics;
  std::string key = GetStorageKey(specifics);

  specifics.set_value(std::string("\0", 1));
  EXPECT_NE(key, GetStorageKey(specifics));
}

// The storage key should never accidentally change for existing data. This
// would cause lookups to fail and either lose or duplicate user data. It should
// be possible for the model type to migrate storage key formats, but doing so
// would need to be done very carefully.
TEST_F(AutocompleteSyncBridgeTest, GetStorageKeyFixed) {
  EXPECT_EQ("\n\x6name 1\x12\avalue 1", GetStorageKey(CreateSpecifics(1)));
  EXPECT_EQ("\n\x6name 2\x12\avalue 2", GetStorageKey(CreateSpecifics(2)));
  // This literal contains the null terminating character, which causes
  // std::string to stop copying early if we don't tell it how much to read.
  EXPECT_EQ(std::string("\n\0\x12\0", 4), GetStorageKey(AutofillSpecifics()));
  AutofillSpecifics specifics;
  specifics.set_name("\xEC\xA4\x91");
  specifics.set_value("\xD0\x80");
  EXPECT_EQ("\n\x3\xEC\xA4\x91\x12\x2\xD0\x80", GetStorageKey(specifics));
}

TEST_F(AutocompleteSyncBridgeTest, GetData) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1);
  const AutofillSpecifics specifics2 = CreateSpecifics(2);
  const AutofillSpecifics specifics3 = CreateSpecifics(3);
  SaveSpecificsToTable({specifics1, specifics2, specifics3});

  const std::map<std::string, AutofillSpecifics> expected{
      {GetStorageKey(specifics1), specifics1},
      {GetStorageKey(specifics3), specifics3}};
  bridge()->GetData({GetStorageKey(specifics1), GetStorageKey(specifics3)},
                    base::Bind(&VerifyDataBatch, expected));
}

TEST_F(AutocompleteSyncBridgeTest, GetDataNotExist) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1);
  const AutofillSpecifics specifics2 = CreateSpecifics(2);
  const AutofillSpecifics specifics3 = CreateSpecifics(3);
  SaveSpecificsToTable({specifics1, specifics2});

  const std::map<std::string, AutofillSpecifics> expected{
      {GetStorageKey(specifics1), specifics1},
      {GetStorageKey(specifics2), specifics2}};
  bridge()->GetData({GetStorageKey(specifics1), GetStorageKey(specifics2),
                     GetStorageKey(specifics3)},
                    base::Bind(&VerifyDataBatch, expected));
}

TEST_F(AutocompleteSyncBridgeTest, GetAllData) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1);
  const AutofillSpecifics specifics2 = CreateSpecifics(2);
  const AutofillSpecifics specifics3 = CreateSpecifics(3);
  SaveSpecificsToTable({specifics1, specifics2, specifics3});

  const std::map<std::string, AutofillSpecifics> expected{
      {GetStorageKey(specifics1), specifics1},
      {GetStorageKey(specifics2), specifics2},
      {GetStorageKey(specifics3), specifics3}};
  bridge()->GetAllData(base::Bind(&VerifyDataBatch, expected));
}

}  // namespace autofill

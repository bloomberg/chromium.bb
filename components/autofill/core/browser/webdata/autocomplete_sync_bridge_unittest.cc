// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"

#include <algorithm>
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
#include "components/sync/model/model_error.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ScopedTempDir;
using base::Time;
using sync_pb::AutofillSpecifics;
using sync_pb::EntitySpecifics;
using syncer::DataBatch;
using syncer::EntityData;
using syncer::EntityDataPtr;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::FakeModelTypeChangeProcessor;
using syncer::KeyAndData;
using syncer::ModelError;
using syncer::ModelType;
using syncer::ModelTypeChangeProcessor;
using syncer::ModelTypeSyncBridge;

namespace autofill {

namespace {

const char kNameFormat[] = "name %d";
const char kValueFormat[] = "value %d";

void VerifyEqual(const AutofillSpecifics& s1, const AutofillSpecifics& s2) {
  // Instead of just comparing serialized strings, manually check fields to show
  // differences on failure.
  EXPECT_EQ(s1.name(), s2.name());
  EXPECT_EQ(s1.value(), s2.value());
  EXPECT_EQ(s1.usage_timestamp().size(), s2.usage_timestamp().size());
  int size = std::min(s1.usage_timestamp().size(), s2.usage_timestamp().size());
  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(s1.usage_timestamp(i), s2.usage_timestamp(i))
        << "Values differ at index " << i;
  }
  // Neither should have any profile data, so we don't have to compare it.
  EXPECT_FALSE(s1.has_profile());
  EXPECT_FALSE(s2.has_profile());
}

void VerifyDataBatch(std::map<std::string, AutofillSpecifics> expected,
                     std::unique_ptr<DataBatch> batch) {
  while (batch->HasNext()) {
    const KeyAndData& pair = batch->Next();
    auto iter = expected.find(pair.first);
    ASSERT_NE(iter, expected.end());
    VerifyEqual(iter->second, pair.second->specifics.autofill());
    // Removing allows us to verify we don't see the same item multiple times,
    // and that we saw everything we expected.
    expected.erase(iter);
  }
  EXPECT_TRUE(expected.empty());
}

std::unique_ptr<ModelTypeChangeProcessor> CreateModelTypeChangeProcessor(
    ModelType type,
    ModelTypeSyncBridge* bridge) {
  return base::MakeUnique<FakeModelTypeChangeProcessor>();
}

AutofillEntry CreateAutofillEntry(
    const sync_pb::AutofillSpecifics& autofill_specifics) {
  AutofillKey key(base::UTF8ToUTF16(autofill_specifics.name()),
                  base::UTF8ToUTF16(autofill_specifics.value()));
  Time date_created, date_last_used;
  const google::protobuf::RepeatedField<int64_t>& timestamps =
      autofill_specifics.usage_timestamp();
  if (!timestamps.empty()) {
    date_created = Time::FromInternalValue(*timestamps.begin());
    date_last_used = Time::FromInternalValue(*timestamps.rbegin());
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
  void NotifyThatSyncHasStarted(ModelType model_type) override {}
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

  AutofillSpecifics CreateSpecifics(const std::string& name,
                                    const std::string& value,
                                    const std::vector<int>& timestamps) {
    AutofillSpecifics specifics;
    specifics.set_name(name);
    specifics.set_value(value);
    for (int timestamp : timestamps) {
      specifics.add_usage_timestamp(
          Time::FromTimeT(timestamp).ToInternalValue());
    }
    return specifics;
  }

  AutofillSpecifics CreateSpecifics(int suffix,
                                    const std::vector<int>& timestamps) {
    return CreateSpecifics(base::StringPrintf(kNameFormat, suffix),
                           base::StringPrintf(kValueFormat, suffix),
                           timestamps);
  }

  AutofillSpecifics CreateSpecifics(int suffix) {
    return CreateSpecifics(suffix, std::vector<int>{0});
  }

  std::string GetStorageKey(const AutofillSpecifics& specifics) {
    std::string key =
        bridge()->GetStorageKey(SpecificsToEntity(specifics).value());
    EXPECT_FALSE(key.empty());
    return key;
  }

  EntityChangeList EntityAddList(
      const std::vector<AutofillSpecifics>& specifics_vector) {
    EntityChangeList changes;
    for (const auto& specifics : specifics_vector) {
      changes.push_back(EntityChange::CreateAdd(GetStorageKey(specifics),
                                                SpecificsToEntity(specifics)));
    }
    return changes;
  }

  void VerifyApplyChanges(const std::vector<EntityChange>& changes) {
    const auto error = bridge()->ApplySyncChanges(
        bridge()->CreateMetadataChangeList(), changes);
    EXPECT_FALSE(error);
  }

  void VerifyApplyAdds(const std::vector<AutofillSpecifics>& specifics) {
    VerifyApplyChanges(EntityAddList(specifics));
  }

  std::map<std::string, AutofillSpecifics> ExpectedMap(
      const std::vector<AutofillSpecifics>& specifics_vector) {
    std::map<std::string, AutofillSpecifics> map;
    for (const auto& specifics : specifics_vector) {
      map[GetStorageKey(specifics)] = specifics;
    }
    return map;
  }

  void VerifyAllData(const std::vector<AutofillSpecifics>& expected) {
    bridge()->GetAllData(base::Bind(&VerifyDataBatch, ExpectedMap(expected)));
  }

 private:
  ScopedTempDir temp_dir_;
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
  bridge()->GetData(
      {GetStorageKey(specifics1), GetStorageKey(specifics3)},
      base::Bind(&VerifyDataBatch, ExpectedMap({specifics1, specifics3})));
}

TEST_F(AutocompleteSyncBridgeTest, GetDataNotExist) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1);
  const AutofillSpecifics specifics2 = CreateSpecifics(2);
  const AutofillSpecifics specifics3 = CreateSpecifics(3);
  SaveSpecificsToTable({specifics1, specifics2});
  bridge()->GetData(
      {GetStorageKey(specifics1), GetStorageKey(specifics2),
       GetStorageKey(specifics3)},
      base::Bind(&VerifyDataBatch, ExpectedMap({specifics1, specifics2})));
}

TEST_F(AutocompleteSyncBridgeTest, GetAllData) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1);
  const AutofillSpecifics specifics2 = CreateSpecifics(2);
  const AutofillSpecifics specifics3 = CreateSpecifics(3);
  SaveSpecificsToTable({specifics1, specifics2, specifics3});
  VerifyAllData({specifics1, specifics2, specifics3});
}

TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesEmpty) {
  // TODO(skym, crbug.com/672619): Ideally would like to verify that the db is
  // not accessed.
  VerifyApplyAdds(std::vector<AutofillSpecifics>());
}

TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesSimple) {
  AutofillSpecifics specifics1 = CreateSpecifics(1);
  AutofillSpecifics specifics2 = CreateSpecifics(2);
  ASSERT_NE(specifics1.SerializeAsString(), specifics2.SerializeAsString());
  ASSERT_NE(GetStorageKey(specifics1), GetStorageKey(specifics2));

  VerifyApplyAdds({specifics1, specifics2});
  VerifyAllData({specifics1, specifics2});

  VerifyApplyChanges({EntityChange::CreateDelete(GetStorageKey(specifics1))});
  VerifyAllData({specifics2});
}

// Should be resilient to deleting and updating non-existent things, and adding
// existing ones.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesWrongChangeType) {
  AutofillSpecifics specifics = CreateSpecifics(1, {1});
  VerifyApplyChanges({EntityChange::CreateDelete(GetStorageKey(specifics))});
  VerifyAllData(std::vector<AutofillSpecifics>());

  VerifyApplyChanges({EntityChange::CreateUpdate(
      GetStorageKey(specifics), SpecificsToEntity(specifics))});
  VerifyAllData({specifics});

  specifics.add_usage_timestamp(Time::FromTimeT(2).ToInternalValue());
  VerifyApplyAdds({specifics});
  VerifyAllData({specifics});
}

// The format in the table has a fixed 2 timestamp format. Round tripping is
// lossy and the middle value should be thrown out.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesThreeTimestamps) {
  VerifyApplyAdds({CreateSpecifics(1, {1, 2, 3})});
  VerifyAllData({CreateSpecifics(1, {1, 3})});
}

// The correct format of timestamps is that the first should be smaller and the
// second should be larger. Bad foreign data should be repaired.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesWrongOrder) {
  VerifyApplyAdds({CreateSpecifics(1, {3, 2})});
  VerifyAllData({CreateSpecifics(1, {2, 3})});
}

// In a minor attempt to save bandwidth, we only send one of the two timestamps
// when they share a value.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesRepeatedTime) {
  VerifyApplyAdds({CreateSpecifics(1, {2, 2})});
  VerifyAllData({CreateSpecifics(1, {2})});
}

// Again, the format in the table is lossy, and cannot distinguish between no
// time, and valid time zero.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesNoTime) {
  VerifyApplyAdds({CreateSpecifics(1, std::vector<int>())});
  VerifyAllData({CreateSpecifics(1, {0})});
}

// If has_value() returns false, then the specifics are determined to be old
// style and ignored.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesNoValue) {
  AutofillSpecifics input = CreateSpecifics(1, {2, 3});
  input.clear_value();
  VerifyApplyAdds({input});
  VerifyAllData(std::vector<AutofillSpecifics>());
}

// Should be treated the same as an empty string name. This inconsistency is
// being perpetuated from the previous sync integration.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesNoName) {
  AutofillSpecifics input = CreateSpecifics(1, {2, 3});
  input.clear_name();
  VerifyApplyAdds({input});
  VerifyAllData({input});
}

// UTF-8 characters should not be dropped when round tripping, including middle
// of string \0 characters.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesUTF) {
  const AutofillSpecifics specifics =
      CreateSpecifics(std::string("\n\0\x12\0", 4), "\xEC\xA4\x91", {1});
  VerifyApplyAdds({specifics});
  VerifyAllData({specifics});
}

// Timestamps should always use the oldest creation time, and the most recent
// usage time.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesMinMaxTimestamps) {
  const AutofillSpecifics initial = CreateSpecifics(1, {3, 6});
  VerifyApplyAdds({initial});
  VerifyAllData({initial});

  VerifyApplyAdds({CreateSpecifics(1, {2, 5})});
  VerifyAllData({CreateSpecifics(1, {2, 6})});

  VerifyApplyAdds({CreateSpecifics(1, {4, 7})});
  VerifyAllData({CreateSpecifics(1, {2, 7})});
}

// An error should be generated when parsing the storage key happens. This
// should never happen in practice because storage keys should be generated at
// runtime by the bridge and not loaded from disk.
TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesBadStorageKey) {
  const auto error = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      {EntityChange::CreateDelete("bogus storage key")});
  EXPECT_TRUE(error);
}

TEST_F(AutocompleteSyncBridgeTest, ApplySyncChangesDatabaseFailure) {
  // TODO(skym, crbug.com/672619): Should have tests that get false back when
  // making db calls and verify the errors are propagated up.
}

}  // namespace autofill

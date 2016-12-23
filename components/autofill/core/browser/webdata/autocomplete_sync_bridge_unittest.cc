// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"

#include <memory>

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
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::AutofillSpecifics;
using syncer::SyncError;

namespace autofill {

namespace {

const char kNameFormat[] = "name %d";
const char kValueFormat[] = "value %d";

void VerifyEqual(const AutofillSpecifics& s1, const AutofillSpecifics& s2) {
  EXPECT_EQ(s1.SerializeAsString(), s2.SerializeAsString());
}

void VerifyDataBatch(std::map<std::string, AutofillSpecifics> expected,
                     SyncError error,
                     std::unique_ptr<syncer::DataBatch> batch) {
  EXPECT_FALSE(error.IsSet());
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
      new_entries.push_back(
          AutocompleteSyncBridge::CreateAutofillEntry(specifics));
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
    return net::EscapePath(specifics.name()) + "|" +
           net::EscapePath(specifics.value());
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

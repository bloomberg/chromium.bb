// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_settings.h"
#include "chrome/browser/extensions/extension_settings_storage_cache.h"
#include "chrome/browser/extensions/extension_settings_noop_storage.h"
#include "chrome/browser/extensions/extension_settings_sync_util.h"
#include "chrome/browser/extensions/syncable_extension_settings_storage.h"
#include "chrome/browser/sync/api/sync_change_processor.h"
#include "content/browser/browser_thread.h"

// TODO(kalman): Integration tests for sync.

namespace {

// Gets the pretty-printed JSON for a value.
static std::string GetJson(const Value& value) {
  std::string json;
  base::JSONWriter::Write(&value, true, &json);
  return json;
}

// Returns whether two Values are equal.
testing::AssertionResult ValuesEq(
    const char* expected_expr,
    const char* actual_expr,
    const Value* expected,
    const Value* actual) {
  if (expected == actual) {
    return testing::AssertionSuccess();
  }
  if (!expected && actual) {
    return testing::AssertionFailure() <<
        "Expected NULL, actual: " << GetJson(*actual);
  }
  if (expected && !actual) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJson(*expected) << ", actual NULL";
  }
  if (!expected->Equals(actual)) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJson(*expected) << ", actual: " << GetJson(*actual);
  }
  return testing::AssertionSuccess();
}

// Returns whether the result of a storage operation is an expected value.
// Logs when different.
testing::AssertionResult SettingsEq(
    const char* expected_expr,
    const char* actual_expr,
    const DictionaryValue* expected,
    const ExtensionSettingsStorage::Result actual) {
  if (actual.HasError()) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJson(*expected) <<
        ", actual has error: " << actual.GetError();
  }
  return ValuesEq(
      expected_expr, actual_expr, expected, actual.GetSettings());
}

// SyncChangeProcessor which just records the changes made, accessed after
// being converted to the more useful ExtensionSettingSyncData via changes().
class MockSyncChangeProcessor : public SyncChangeProcessor {
 public:
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE {
    for (SyncChangeList::const_iterator it = change_list.begin();
        it != change_list.end(); ++it) {
      changes_.push_back(ExtensionSettingSyncData(*it));
    }
    return SyncError();
  }

  const ExtensionSettingSyncDataList& changes() { return changes_; }

  void ClearChanges() {
    changes_.clear();
  }

  // Returns the only change for a given extension setting.  If there is not
  // exactly 1 change for that key, a test assertion will fail.
  ExtensionSettingSyncData GetOnlyChange(
      const std::string& extension_id, const std::string& key) {
    ExtensionSettingSyncDataList matching_changes;
    for (ExtensionSettingSyncDataList::iterator it = changes_.begin();
        it != changes_.end(); ++it) {
      if (it->extension_id() == extension_id && it->key() == key) {
        matching_changes.push_back(*it);
      }
    }
    if (matching_changes.empty()) {
      ADD_FAILURE() << "No matching changes for " << extension_id << "/" <<
          key << " (out of " << changes_.size() << ")";
      return ExtensionSettingSyncData(SyncChange::ACTION_INVALID, "", "", NULL);
    }
    if (matching_changes.size() != 1u) {
      ADD_FAILURE() << matching_changes.size() << " matching changes for " <<
           extension_id << "/" << key << " (out of " << changes_.size() << ")";
    }
    return matching_changes[0];
  }

 private:
  ExtensionSettingSyncDataList changes_;
};

}  // namespace

class ExtensionSettingsSyncTest : public testing::Test {
 public:
  ExtensionSettingsSyncTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    settings_.reset(new ExtensionSettings(temp_dir_.path()));
  }

  virtual void TearDown() OVERRIDE {
    // Must do this explicitly here so that it's destroyed before the
    // message loops are.
    settings_.reset();
  }

 protected:
  // Creates a new extension storage object and adds a record of the extension
  // to the extension service.
  SyncableExtensionSettingsStorage* GetStorage(
      const std::string& extension_id) {
    return static_cast<SyncableExtensionSettingsStorage*>(
        settings_->GetStorage(extension_id));
  }

  MockSyncChangeProcessor sync_;
  scoped_ptr<ExtensionSettings> settings_;

  // Gets all the sync data from settings_ as a map from extension id to its
  // sync data.
  std::map<std::string, ExtensionSettingSyncDataList> GetAllSyncData() {
    SyncDataList as_list =
        settings_->GetAllSyncData(syncable::EXTENSION_SETTINGS);
    std::map<std::string, ExtensionSettingSyncDataList> as_map;
    for (SyncDataList::iterator it = as_list.begin();
        it != as_list.end(); ++it) {
      ExtensionSettingSyncData sync_data(*it);
      as_map[sync_data.extension_id()].push_back(sync_data);
    }
    return as_map;
  }

 private:
  ScopedTempDir temp_dir_;

  // Need these so that the DCHECKs for running on FILE or UI threads pass.
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_F(ExtensionSettingsSyncTest, NoDataDoesNotInvokeSync) {
  ASSERT_EQ(0u, GetAllSyncData().size());

  // Have one extension created before sync is set up, the other created after.
  GetStorage("s1");
  ASSERT_EQ(0u, GetAllSyncData().size());

  settings_->MergeDataAndStartSyncing(
      syncable::EXTENSION_SETTINGS,
      SyncDataList(),
      &sync_);

  GetStorage("s2");
  ASSERT_EQ(0u, GetAllSyncData().size());

  settings_->StopSyncing(syncable::EXTENSION_SETTINGS);

  ASSERT_EQ(0u, sync_.changes().size());
  ASSERT_EQ(0u, GetAllSyncData().size());
}

TEST_F(ExtensionSettingsSyncTest, InSyncDataDoesNotInvokeSync) {
  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  SyncableExtensionSettingsStorage* storage1 = GetStorage("s1");
  SyncableExtensionSettingsStorage* storage2 = GetStorage("s2");

  storage1->Set("foo", value1);
  storage2->Set("bar", value2);

  std::map<std::string, ExtensionSettingSyncDataList> all_sync_data =
      GetAllSyncData();
  ASSERT_EQ(2u, all_sync_data.size());
  ASSERT_EQ(1u, all_sync_data["s1"].size());
  ASSERT_PRED_FORMAT2(ValuesEq, &value1, &all_sync_data["s1"][0].value());
  ASSERT_EQ(1u, all_sync_data["s2"].size());
  ASSERT_PRED_FORMAT2(ValuesEq, &value2, &all_sync_data["s2"][0].value());

  SyncDataList sync_data;
  sync_data.push_back(extension_settings_sync_util::CreateData(
      "s1", "foo", value1));
  sync_data.push_back(extension_settings_sync_util::CreateData(
      "s2", "bar", value2));

  settings_->MergeDataAndStartSyncing(
      syncable::EXTENSION_SETTINGS, sync_data, &sync_);
  settings_->StopSyncing(syncable::EXTENSION_SETTINGS);

  // Already in sync, so no changes.
  ASSERT_EQ(0u, sync_.changes().size());
}

TEST_F(ExtensionSettingsSyncTest, LocalDataWithNoSyncDataIsPushedToSync) {
  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  SyncableExtensionSettingsStorage* storage1 = GetStorage("s1");
  SyncableExtensionSettingsStorage* storage2 = GetStorage("s2");

  storage1->Set("foo", value1);
  storage2->Set("bar", value2);

  settings_->MergeDataAndStartSyncing(
      syncable::EXTENSION_SETTINGS, SyncDataList(), &sync_);

  // All settings should have been pushed to sync.
  ASSERT_EQ(2u, sync_.changes().size());
  ExtensionSettingSyncData change = sync_.GetOnlyChange("s1", "foo");
  ASSERT_EQ(SyncChange::ACTION_ADD, change.change_type());
  ASSERT_TRUE(value1.Equals(&change.value()));
  change = sync_.GetOnlyChange("s2", "bar");
  ASSERT_EQ(SyncChange::ACTION_ADD, change.change_type());
  ASSERT_TRUE(value2.Equals(&change.value()));

  settings_->StopSyncing(syncable::EXTENSION_SETTINGS);
}

TEST_F(ExtensionSettingsSyncTest, AnySyncDataOverwritesLocalData) {
  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  // Maintain dictionaries mirrored to the expected values of the settings in
  // each storage area.
  DictionaryValue expected1, expected2;

  // Pre-populate one of the storage areas.
  SyncableExtensionSettingsStorage* storage1 = GetStorage("s1");
  storage1->Set("overwriteMe", value1);

  SyncDataList sync_data;
  sync_data.push_back(extension_settings_sync_util::CreateData(
      "s1", "foo", value1));
  sync_data.push_back(extension_settings_sync_util::CreateData(
      "s2", "bar", value2));
  settings_->MergeDataAndStartSyncing(
      syncable::EXTENSION_SETTINGS, sync_data, &sync_);
  expected1.Set("foo", value1.DeepCopy());
  expected2.Set("bar", value2.DeepCopy());

  SyncableExtensionSettingsStorage* storage2 = GetStorage("s2");

  // All changes should be local, so no sync changes.
  ASSERT_EQ(0u, sync_.changes().size());

  // Sync settings should have been pushed to local settings.
  ASSERT_PRED_FORMAT2(SettingsEq, &expected1, storage1->Get());
  ASSERT_PRED_FORMAT2(SettingsEq, &expected2, storage2->Get());

  settings_->StopSyncing(syncable::EXTENSION_SETTINGS);
}

TEST_F(ExtensionSettingsSyncTest, ProcessSyncChanges) {
  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  // Maintain dictionaries mirrored to the expected values of the settings in
  // each storage area.
  DictionaryValue expected1, expected2;

  // Make storage1 initialised from local data, storage2 initialised from sync.
  SyncableExtensionSettingsStorage* storage1 = GetStorage("s1");
  SyncableExtensionSettingsStorage* storage2 = GetStorage("s2");

  storage1->Set("foo", value1);
  expected1.Set("foo", value1.DeepCopy());

  SyncDataList sync_data;
  sync_data.push_back(extension_settings_sync_util::CreateData(
      "s2", "bar", value2));

  settings_->MergeDataAndStartSyncing(
      syncable::EXTENSION_SETTINGS, sync_data, &sync_);
  expected2.Set("bar", value2.DeepCopy());

  // Make sync add some settings.
  SyncChangeList change_list;
  change_list.push_back(extension_settings_sync_util::CreateAdd(
      "s1", "bar", value2));
  change_list.push_back(extension_settings_sync_util::CreateAdd(
      "s2", "foo", value1));
  settings_->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Set("bar", value2.DeepCopy());
  expected2.Set("foo", value1.DeepCopy());

  ASSERT_PRED_FORMAT2(SettingsEq, &expected1, storage1->Get());
  ASSERT_PRED_FORMAT2(SettingsEq, &expected2, storage2->Get());

  // Make sync update some settings, storage1 the new setting, storage2 the
  // initial setting.
  change_list.clear();
  change_list.push_back(extension_settings_sync_util::CreateUpdate(
      "s1", "bar", value2));
  change_list.push_back(extension_settings_sync_util::CreateUpdate(
      "s2", "bar", value1));
  settings_->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Set("bar", value2.DeepCopy());
  expected2.Set("bar", value1.DeepCopy());

  ASSERT_PRED_FORMAT2(SettingsEq, &expected1, storage1->Get());
  ASSERT_PRED_FORMAT2(SettingsEq, &expected2, storage2->Get());

  // Make sync remove some settings, storage1 the initial setting, storage2 the
  // new setting.
  change_list.clear();
  change_list.push_back(extension_settings_sync_util::CreateDelete(
      "s1", "foo"));
  change_list.push_back(extension_settings_sync_util::CreateDelete(
      "s2", "foo"));
  settings_->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Remove("foo", NULL);
  expected2.Remove("foo", NULL);

  ASSERT_PRED_FORMAT2(SettingsEq, &expected1, storage1->Get());
  ASSERT_PRED_FORMAT2(SettingsEq, &expected2, storage2->Get());

  settings_->StopSyncing(syncable::EXTENSION_SETTINGS);
}

TEST_F(ExtensionSettingsSyncTest, PushToSync) {
  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  // Make storage1/2 initialised from local data, storage3/4 initialised from
  // sync.
  SyncableExtensionSettingsStorage* storage1 = GetStorage("s1");
  SyncableExtensionSettingsStorage* storage2 = GetStorage("s2");
  SyncableExtensionSettingsStorage* storage3 = GetStorage("s3");
  SyncableExtensionSettingsStorage* storage4 = GetStorage("s4");

  storage1->Set("foo", value1);
  storage2->Set("foo", value1);

  SyncDataList sync_data;
  sync_data.push_back(extension_settings_sync_util::CreateData(
      "s3", "bar", value2));
  sync_data.push_back(extension_settings_sync_util::CreateData(
      "s4", "bar", value2));

  settings_->MergeDataAndStartSyncing(
      syncable::EXTENSION_SETTINGS, sync_data, &sync_);

  // Add something locally.
  storage1->Set("bar", value2);
  storage2->Set("bar", value2);
  storage3->Set("foo", value1);
  storage4->Set("foo", value1);

  ExtensionSettingSyncData change = sync_.GetOnlyChange("s1", "bar");
    ASSERT_EQ(SyncChange::ACTION_ADD, change.change_type());
    ASSERT_TRUE(value2.Equals(&change.value()));
  sync_.GetOnlyChange("s2", "bar");
    ASSERT_EQ(SyncChange::ACTION_ADD, change.change_type());
    ASSERT_TRUE(value2.Equals(&change.value()));
  change = sync_.GetOnlyChange("s3", "foo");
    ASSERT_EQ(SyncChange::ACTION_ADD, change.change_type());
    ASSERT_TRUE(value1.Equals(&change.value()));
  change = sync_.GetOnlyChange("s4", "foo");
    ASSERT_EQ(SyncChange::ACTION_ADD, change.change_type());
    ASSERT_TRUE(value1.Equals(&change.value()));

  // Change something locally, storage1/3 the new setting and storage2/4 the
  // initial setting, for all combinations of local vs sync intialisation and
  // new vs initial.
  sync_.ClearChanges();
  storage1->Set("bar", value1);
  storage2->Set("foo", value2);
  storage3->Set("bar", value1);
  storage4->Set("foo", value2);

  change = sync_.GetOnlyChange("s1", "bar");
    ASSERT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    ASSERT_TRUE(value1.Equals(&change.value()));
  change = sync_.GetOnlyChange("s2", "foo");
    ASSERT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    ASSERT_TRUE(value2.Equals(&change.value()));
  change = sync_.GetOnlyChange("s3", "bar");
    ASSERT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    ASSERT_TRUE(value1.Equals(&change.value()));
  change = sync_.GetOnlyChange("s4", "foo");
    ASSERT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    ASSERT_TRUE(value2.Equals(&change.value()));

  // Remove something locally, storage1/3 the new setting and storage2/4 the
  // initial setting, for all combinations of local vs sync intialisation and
  // new vs initial.
  sync_.ClearChanges();
  storage1->Remove("foo");
  storage2->Remove("bar");
  storage3->Remove("foo");
  storage4->Remove("bar");

  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s1", "foo").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s2", "bar").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s3", "foo").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s4", "bar").change_type());

  // Remove some nonexistent settings.
  sync_.ClearChanges();
  storage1->Remove("foo");
  storage2->Remove("bar");
  storage3->Remove("foo");
  storage4->Remove("bar");

  ASSERT_EQ(0u, sync_.changes().size());

  // Clear the rest of the settings.  Add the removed ones back first so that
  // more than one setting is cleared.
  storage1->Set("foo", value1);
  storage2->Set("bar", value2);
  storage3->Set("foo", value1);
  storage4->Set("bar", value2);

  sync_.ClearChanges();
  storage1->Clear();
  storage2->Clear();
  storage3->Clear();
  storage4->Clear();

  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s1", "foo").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s1", "bar").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s2", "foo").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s2", "bar").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s3", "foo").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s3", "bar").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s4", "foo").change_type());
  ASSERT_EQ(
      SyncChange::ACTION_DELETE,
      sync_.GetOnlyChange("s4", "bar").change_type());

  settings_->StopSyncing(syncable::EXTENSION_SETTINGS);
}

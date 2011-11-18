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
#include "chrome/browser/extensions/settings/failing_settings_storage.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/extensions/settings/settings_storage_cache.h"
#include "chrome/browser/extensions/settings/settings_storage_factory.h"
#include "chrome/browser/extensions/settings/settings_sync_util.h"
#include "chrome/browser/extensions/settings/settings_test_util.h"
#include "chrome/browser/extensions/settings/syncable_settings_storage.h"
#include "chrome/browser/extensions/settings/testing_settings_storage.h"
#include "chrome/browser/sync/api/sync_change_processor.h"
#include "content/test/test_browser_thread.h"

// TODO(kalman): Integration tests for sync.

using content::BrowserThread;

namespace extensions {

using namespace settings_test_util;

namespace {

// Gets the pretty-printed JSON for a value.
static std::string GetJson(const Value& value) {
  std::string json;
  base::JSONWriter::Write(&value, true, &json);
  return json;
}

// Returns whether two Values are equal.
testing::AssertionResult ValuesEq(
    const char* _1, const char* _2,
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
    const char* _1, const char* _2,
    const DictionaryValue& expected,
    const SettingsStorage::ReadResult& actual) {
  if (actual.HasError()) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJson(expected) <<
        ", actual has error: " << actual.error();
  }
  return ValuesEq(_1, _2, &expected, &actual.settings());
}

// SyncChangeProcessor which just records the changes made, accessed after
// being converted to the more useful SettingSyncData via changes().
class MockSyncChangeProcessor : public SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() : fail_all_requests_(false) {}

  // SyncChangeProcessor implementation.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE {
    if (fail_all_requests_) {
      return SyncError(
          FROM_HERE,
          "MockSyncChangeProcessor: configured to fail",
          change_list[0].sync_data().GetDataType());
    }
    for (SyncChangeList::const_iterator it = change_list.begin();
        it != change_list.end(); ++it) {
      changes_.push_back(SettingSyncData(*it));
    }
    return SyncError();
  }

  // Mock methods.

  const SettingSyncDataList& changes() { return changes_; }

  void ClearChanges() {
    changes_.clear();
  }

  void SetFailAllRequests(bool fail_all_requests) {
    fail_all_requests_ = fail_all_requests;
  }

  // Returns the only change for a given extension setting.  If there is not
  // exactly 1 change for that key, a test assertion will fail.
  SettingSyncData GetOnlyChange(
      const std::string& extension_id, const std::string& key) {
    SettingSyncDataList matching_changes;
    for (SettingSyncDataList::iterator it = changes_.begin();
        it != changes_.end(); ++it) {
      if (it->extension_id() == extension_id && it->key() == key) {
        matching_changes.push_back(*it);
      }
    }
    if (matching_changes.empty()) {
      ADD_FAILURE() << "No matching changes for " << extension_id << "/" <<
          key << " (out of " << changes_.size() << ")";
      return SettingSyncData(
          SyncChange::ACTION_INVALID, "", "", new DictionaryValue());
    }
    if (matching_changes.size() != 1u) {
      ADD_FAILURE() << matching_changes.size() << " matching changes for " <<
           extension_id << "/" << key << " (out of " << changes_.size() << ")";
    }
    return matching_changes[0];
  }

 private:
  SettingSyncDataList changes_;
  bool fail_all_requests_;
};

// SettingsStorageFactory which always returns TestingSettingsStorage objects,
// and allows individually created objects to be returned.
class TestingSettingsStorageFactory : public SettingsStorageFactory {
 public:
  TestingSettingsStorage* GetExisting(const std::string& extension_id) {
    DCHECK(created_.count(extension_id));
    return created_[extension_id];
  }

  // SettingsStorageFactory implementation.
  virtual SettingsStorage* Create(
      const FilePath& base_path, const std::string& extension_id) OVERRIDE {
    TestingSettingsStorage* new_storage = new TestingSettingsStorage();
    DCHECK(!created_.count(extension_id));
    created_[extension_id] = new_storage;
    return new_storage;
  }

 private:
  // None of these storage areas are owned by this factory, so care must be
  // taken when calling GetExisting.
  std::map<std::string, TestingSettingsStorage*> created_;
};

void AssignSettingsService(SyncableService** dst, SyncableService* src) {
  *dst = src;
}

}  // namespace

class ExtensionSettingsSyncTest : public testing::Test {
 public:
  ExtensionSettingsSyncTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new MockProfile(temp_dir_.path()));
    storage_factory_ =
        new ScopedSettingsStorageFactory(new SettingsLeveldbStorage::Factory());
    frontend_.reset(SettingsFrontend::Create(storage_factory_, profile_.get()));
  }

  virtual void TearDown() OVERRIDE {
    frontend_.reset();
    profile_.reset();
  }

 protected:
  // Adds a record of an extension or app to the extension service, then returns
  // its storage area.
  SettingsStorage* AddExtensionAndGetStorage(
      const std::string& id, Extension::Type type) {
    profile_->GetMockExtensionService()->AddExtensionWithId(id, type);
    return GetStorage(id, frontend_.get());
  }

  // Gets the SyncableService for the given sync type.
  SyncableService* GetSyncableService(syncable::ModelType model_type) {
    SyncableService* settings_service = NULL;
    frontend_->RunWithSyncableService(
        model_type, base::Bind(&AssignSettingsService, &settings_service));
    MessageLoop::current()->RunAllPending();
    return settings_service;
  }

  // Gets all the sync data from the SyncableService for a sync type as a map
  // from extension id to its sync data.
  std::map<std::string, SettingSyncDataList> GetAllSyncData(
      syncable::ModelType model_type) {
    SyncDataList as_list =
        GetSyncableService(model_type)->GetAllSyncData(model_type);
    std::map<std::string, SettingSyncDataList> as_map;
    for (SyncDataList::iterator it = as_list.begin();
        it != as_list.end(); ++it) {
      SettingSyncData sync_data(*it);
      as_map[sync_data.extension_id()].push_back(sync_data);
    }
    return as_map;
  }

  // Need these so that the DCHECKs for running on FILE or UI threads pass.
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  ScopedTempDir temp_dir_;
  MockSyncChangeProcessor sync_;
  scoped_ptr<MockProfile> profile_;
  scoped_ptr<SettingsFrontend> frontend_;

  // Owned by |frontend_|.
  ScopedSettingsStorageFactory* storage_factory_;
};

// Get a semblance of coverage for both EXTENSION_SETTINGS and APP_SETTINGS
// sync by roughly alternative which one to test.

TEST_F(ExtensionSettingsSyncTest, NoDataDoesNotInvokeSync) {
  syncable::ModelType model_type = syncable::EXTENSION_SETTINGS;
  Extension::Type type = Extension::TYPE_EXTENSION;

  ASSERT_EQ(0u, GetAllSyncData(model_type).size());

  // Have one extension created before sync is set up, the other created after.
  AddExtensionAndGetStorage("s1", type);
  ASSERT_EQ(0u, GetAllSyncData(model_type).size());

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);

  AddExtensionAndGetStorage("s2", type);
  ASSERT_EQ(0u, GetAllSyncData(model_type).size());

  GetSyncableService(model_type)->StopSyncing(model_type);

  ASSERT_EQ(0u, sync_.changes().size());
  ASSERT_EQ(0u, GetAllSyncData(model_type).size());
}

TEST_F(ExtensionSettingsSyncTest, InSyncDataDoesNotInvokeSync) {
  syncable::ModelType model_type = syncable::APP_SETTINGS;
  Extension::Type type = Extension::TYPE_PACKAGED_APP;

  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  SettingsStorage* storage1 = AddExtensionAndGetStorage("s1", type);
  SettingsStorage* storage2 = AddExtensionAndGetStorage("s2", type);

  storage1->Set("foo", value1);
  storage2->Set("bar", value2);

  std::map<std::string, SettingSyncDataList> all_sync_data =
      GetAllSyncData(model_type);
  ASSERT_EQ(2u, all_sync_data.size());
  ASSERT_EQ(1u, all_sync_data["s1"].size());
  ASSERT_PRED_FORMAT2(ValuesEq, &value1, &all_sync_data["s1"][0].value());
  ASSERT_EQ(1u, all_sync_data["s2"].size());
  ASSERT_PRED_FORMAT2(ValuesEq, &value2, &all_sync_data["s2"][0].value());

  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s1", "foo", value1));
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2));

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, sync_data, &sync_);

  // Already in sync, so no changes.
  ASSERT_EQ(0u, sync_.changes().size());

  // Regression test: not-changing the synced value shouldn't result in a sync
  // change, and changing the synced value should result in an update.
  storage1->Set("foo", value1);
  ASSERT_EQ(0u, sync_.changes().size());

  storage1->Set("foo", value2);
  ASSERT_EQ(1u, sync_.changes().size());
  SettingSyncData change = sync_.GetOnlyChange("s1", "foo");
  ASSERT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
  ASSERT_TRUE(value2.Equals(&change.value()));

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, LocalDataWithNoSyncDataIsPushedToSync) {
  syncable::ModelType model_type = syncable::EXTENSION_SETTINGS;
  Extension::Type type = Extension::TYPE_EXTENSION;

  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  SettingsStorage* storage1 = AddExtensionAndGetStorage("s1", type);
  SettingsStorage* storage2 = AddExtensionAndGetStorage("s2", type);

  storage1->Set("foo", value1);
  storage2->Set("bar", value2);

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);

  // All settings should have been pushed to sync.
  ASSERT_EQ(2u, sync_.changes().size());
  SettingSyncData change = sync_.GetOnlyChange("s1", "foo");
  ASSERT_EQ(SyncChange::ACTION_ADD, change.change_type());
  ASSERT_TRUE(value1.Equals(&change.value()));
  change = sync_.GetOnlyChange("s2", "bar");
  ASSERT_EQ(SyncChange::ACTION_ADD, change.change_type());
  ASSERT_TRUE(value2.Equals(&change.value()));

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, AnySyncDataOverwritesLocalData) {
  syncable::ModelType model_type = syncable::APP_SETTINGS;
  Extension::Type type = Extension::TYPE_PACKAGED_APP;

  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  // Maintain dictionaries mirrored to the expected values of the settings in
  // each storage area.
  DictionaryValue expected1, expected2;

  // Pre-populate one of the storage areas.
  SettingsStorage* storage1 = AddExtensionAndGetStorage("s1", type);
  storage1->Set("overwriteMe", value1);

  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s1", "foo", value1));
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2));
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, sync_data, &sync_);
  expected1.Set("foo", value1.DeepCopy());
  expected2.Set("bar", value2.DeepCopy());

  SettingsStorage* storage2 = AddExtensionAndGetStorage("s2", type);

  // All changes should be local, so no sync changes.
  ASSERT_EQ(0u, sync_.changes().size());

  // Sync settings should have been pushed to local settings.
  ASSERT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  ASSERT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, ProcessSyncChanges) {
  syncable::ModelType model_type = syncable::EXTENSION_SETTINGS;
  Extension::Type type = Extension::TYPE_EXTENSION;

  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  // Maintain dictionaries mirrored to the expected values of the settings in
  // each storage area.
  DictionaryValue expected1, expected2;

  // Make storage1 initialised from local data, storage2 initialised from sync.
  SettingsStorage* storage1 = AddExtensionAndGetStorage("s1", type);
  SettingsStorage* storage2 = AddExtensionAndGetStorage("s2", type);

  storage1->Set("foo", value1);
  expected1.Set("foo", value1.DeepCopy());

  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2));

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, sync_data, &sync_);
  expected2.Set("bar", value2.DeepCopy());

  // Make sync add some settings.
  SyncChangeList change_list;
  change_list.push_back(settings_sync_util::CreateAdd(
      "s1", "bar", value2));
  change_list.push_back(settings_sync_util::CreateAdd(
      "s2", "foo", value1));
  GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Set("bar", value2.DeepCopy());
  expected2.Set("foo", value1.DeepCopy());

  ASSERT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  ASSERT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  // Make sync update some settings, storage1 the new setting, storage2 the
  // initial setting.
  change_list.clear();
  change_list.push_back(settings_sync_util::CreateUpdate(
      "s1", "bar", value2));
  change_list.push_back(settings_sync_util::CreateUpdate(
      "s2", "bar", value1));
  GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Set("bar", value2.DeepCopy());
  expected2.Set("bar", value1.DeepCopy());

  ASSERT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  ASSERT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  // Make sync remove some settings, storage1 the initial setting, storage2 the
  // new setting.
  change_list.clear();
  change_list.push_back(settings_sync_util::CreateDelete(
      "s1", "foo"));
  change_list.push_back(settings_sync_util::CreateDelete(
      "s2", "foo"));
  GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Remove("foo", NULL);
  expected2.Remove("foo", NULL);

  ASSERT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  ASSERT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, PushToSync) {
  syncable::ModelType model_type = syncable::APP_SETTINGS;
  Extension::Type type = Extension::TYPE_PACKAGED_APP;

  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  // Make storage1/2 initialised from local data, storage3/4 initialised from
  // sync.
  SettingsStorage* storage1 = AddExtensionAndGetStorage("s1", type);
  SettingsStorage* storage2 = AddExtensionAndGetStorage("s2", type);
  SettingsStorage* storage3 = AddExtensionAndGetStorage("s3", type);
  SettingsStorage* storage4 = AddExtensionAndGetStorage("s4", type);

  storage1->Set("foo", value1);
  storage2->Set("foo", value1);

  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s3", "bar", value2));
  sync_data.push_back(settings_sync_util::CreateData(
      "s4", "bar", value2));

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, sync_data, &sync_);

  // Add something locally.
  storage1->Set("bar", value2);
  storage2->Set("bar", value2);
  storage3->Set("foo", value1);
  storage4->Set("foo", value1);

  SettingSyncData change = sync_.GetOnlyChange("s1", "bar");
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

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, ExtensionAndAppSettingsSyncSeparately) {
  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  // storage1 is an extension, storage2 is an app.
  SettingsStorage* storage1 = AddExtensionAndGetStorage(
      "s1", Extension::TYPE_EXTENSION);
  SettingsStorage* storage2 = AddExtensionAndGetStorage(
      "s2", Extension::TYPE_PACKAGED_APP);

  storage1->Set("foo", value1);
  storage2->Set("bar", value2);

  std::map<std::string, SettingSyncDataList> extension_sync_data =
      GetAllSyncData(syncable::EXTENSION_SETTINGS);
  ASSERT_EQ(1u, extension_sync_data.size());
  ASSERT_EQ(1u, extension_sync_data["s1"].size());
  ASSERT_PRED_FORMAT2(ValuesEq, &value1, &extension_sync_data["s1"][0].value());

  std::map<std::string, SettingSyncDataList> app_sync_data =
      GetAllSyncData(syncable::APP_SETTINGS);
  ASSERT_EQ(1u, app_sync_data.size());
  ASSERT_EQ(1u, app_sync_data["s2"].size());
  ASSERT_PRED_FORMAT2(ValuesEq, &value2, &app_sync_data["s2"][0].value());

  // Stop each separately, there should be no changes either time.
  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s1", "foo", value1));

  GetSyncableService(syncable::EXTENSION_SETTINGS)->
      MergeDataAndStartSyncing(syncable::EXTENSION_SETTINGS, sync_data, &sync_);
  GetSyncableService(syncable::EXTENSION_SETTINGS)->
      StopSyncing(syncable::EXTENSION_SETTINGS);
  ASSERT_EQ(0u, sync_.changes().size());

  sync_data.clear();
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2));

  GetSyncableService(syncable::APP_SETTINGS)->
      MergeDataAndStartSyncing(syncable::APP_SETTINGS, sync_data, &sync_);
  GetSyncableService(syncable::APP_SETTINGS)->
      StopSyncing(syncable::APP_SETTINGS);
  ASSERT_EQ(0u, sync_.changes().size());
}

TEST_F(ExtensionSettingsSyncTest, FailingStartSyncingDisablesSync) {
  syncable::ModelType model_type = syncable::EXTENSION_SETTINGS;
  Extension::Type type = Extension::TYPE_EXTENSION;

  StringValue fooValue("fooValue");
  StringValue barValue("barValue");

  // There is a bit of a convoluted method to get storage areas that can fail;
  // hand out TestingSettingsStorage object then toggle them failing/succeeding
  // as necessary.
  TestingSettingsStorageFactory* testing_factory =
      new TestingSettingsStorageFactory();
  storage_factory_->Reset(testing_factory);

  SettingsStorage* good = AddExtensionAndGetStorage("good", type);
  SettingsStorage* bad = AddExtensionAndGetStorage("bad", type);

  // Make bad fail for incoming sync changes.
  testing_factory->GetExisting("bad")->SetFailAllRequests(true);
  {
    SyncDataList sync_data;
    sync_data.push_back(
        settings_sync_util::CreateData("good", "foo", fooValue));
    sync_data.push_back(
        settings_sync_util::CreateData("bad", "foo", fooValue));
    GetSyncableService(model_type)->MergeDataAndStartSyncing(
        model_type, sync_data, &sync_);
  }
  testing_factory->GetExisting("bad")->SetFailAllRequests(false);

  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Changes made to good should be sent to sync, changes from bad shouldn't.
  sync_.ClearChanges();
  good->Set("bar", barValue);
  bad->Set("bar", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(1u, sync_.changes().size());

  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Changes received from sync should go to good but not bad (even when it's
  // not failing).
  {
    SyncChangeList change_list;
    change_list.push_back(
        settings_sync_util::CreateUpdate("good", "foo", barValue));
    // (Sending UPDATE here even though it's adding, since that's what the state
    // of sync is.  In any case, it won't work.)
    change_list.push_back(
        settings_sync_util::CreateUpdate("bad", "foo", barValue));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Changes made to bad still shouldn't go to sync, even though it didn't fail
  // last time.
  sync_.ClearChanges();
  good->Set("bar", fooValue);
  bad->Set("bar", fooValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(1u, sync_.changes().size());

  {
    DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Failing ProcessSyncChanges shouldn't go to the storage.
  testing_factory->GetExisting("bad")->SetFailAllRequests(true);
  {
    SyncChangeList change_list;
    change_list.push_back(
        settings_sync_util::CreateUpdate("good", "foo", fooValue));
    // (Ditto.)
    change_list.push_back(
        settings_sync_util::CreateUpdate("bad", "foo", fooValue));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }
  testing_factory->GetExisting("bad")->SetFailAllRequests(false);

  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Restarting sync should make bad start syncing again.
  sync_.ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);

  // Local settings will have been pushed to sync, since it's empty (in this
  // test; presumably it wouldn't be live, since we've been getting changes).
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("bad", "bar").change_type());
  EXPECT_EQ(3u, sync_.changes().size());

  // Live local changes now get pushed, too.
  sync_.ClearChanges();
  good->Set("bar", barValue);
  bad->Set("bar", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("bad", "bar").change_type());
  EXPECT_EQ(2u, sync_.changes().size());

  // And ProcessSyncChanges work, too.
  {
    SyncChangeList change_list;
    change_list.push_back(
        settings_sync_util::CreateUpdate("good", "bar", fooValue));
    change_list.push_back(
        settings_sync_util::CreateUpdate("bad", "bar", fooValue));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }
}

TEST_F(ExtensionSettingsSyncTest, FailingProcessChangesDisablesSync) {
  // The test above tests a failing ProcessSyncChanges too, but here test with
  // an initially passing MergeDataAndStartSyncing.
  syncable::ModelType model_type = syncable::APP_SETTINGS;
  Extension::Type type = Extension::TYPE_PACKAGED_APP;

  StringValue fooValue("fooValue");
  StringValue barValue("barValue");

  TestingSettingsStorageFactory* testing_factory =
      new TestingSettingsStorageFactory();
  storage_factory_->Reset(testing_factory);

  SettingsStorage* good = AddExtensionAndGetStorage("good", type);
  SettingsStorage* bad = AddExtensionAndGetStorage("bad", type);

  // Unlike before, initially succeeding MergeDataAndStartSyncing.
  {
    SyncDataList sync_data;
    sync_data.push_back(
        settings_sync_util::CreateData("good", "foo", fooValue));
    sync_data.push_back(
        settings_sync_util::CreateData("bad", "foo", fooValue));
    GetSyncableService(model_type)->MergeDataAndStartSyncing(
        model_type, sync_data, &sync_);
  }

  EXPECT_EQ(0u, sync_.changes().size());

  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Now fail ProcessSyncChanges for bad.
  testing_factory->GetExisting("bad")->SetFailAllRequests(true);
  {
    SyncChangeList change_list;
    change_list.push_back(
        settings_sync_util::CreateAdd("good", "bar", barValue));
    change_list.push_back(
        settings_sync_util::CreateAdd("bad", "bar", barValue));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }
  testing_factory->GetExisting("bad")->SetFailAllRequests(false);

  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // No more changes sent to sync for bad.
  sync_.ClearChanges();
  good->Set("foo", barValue);
  bad->Set("foo", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_.changes().size());

  // No more changes received from sync should go to bad.
  {
    SyncChangeList change_list;
    change_list.push_back(
        settings_sync_util::CreateAdd("good", "foo", fooValue));
    change_list.push_back(
        settings_sync_util::CreateAdd("bad", "foo", fooValue));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }
}

TEST_F(ExtensionSettingsSyncTest, FailingGetAllSyncDataDoesntStopSync) {
  syncable::ModelType model_type = syncable::EXTENSION_SETTINGS;
  Extension::Type type = Extension::TYPE_EXTENSION;

  StringValue fooValue("fooValue");
  StringValue barValue("barValue");

  TestingSettingsStorageFactory* testing_factory =
      new TestingSettingsStorageFactory();
  storage_factory_->Reset(testing_factory);

  SettingsStorage* good = AddExtensionAndGetStorage("good", type);
  SettingsStorage* bad = AddExtensionAndGetStorage("bad", type);

  good->Set("foo", fooValue);
  bad->Set("foo", fooValue);

  // Even though bad will fail to get all sync data, sync data should still
  // include that from good.
  testing_factory->GetExisting("bad")->SetFailAllRequests(true);
  {
    SyncDataList all_sync_data =
        GetSyncableService(model_type)->GetAllSyncData(model_type);
    EXPECT_EQ(1u, all_sync_data.size());
    EXPECT_EQ("good/foo", all_sync_data[0].GetTag());
  }
  testing_factory->GetExisting("bad")->SetFailAllRequests(false);

  // Sync shouldn't be disabled for good (nor bad -- but this is unimportant).
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("bad", "foo").change_type());
  EXPECT_EQ(2u, sync_.changes().size());

  sync_.ClearChanges();
  good->Set("bar", barValue);
  bad->Set("bar", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("bad", "bar").change_type());
  EXPECT_EQ(2u, sync_.changes().size());
}

TEST_F(ExtensionSettingsSyncTest, FailureToReadChangesToPushDisablesSync) {
  syncable::ModelType model_type = syncable::APP_SETTINGS;
  Extension::Type type = Extension::TYPE_PACKAGED_APP;

  StringValue fooValue("fooValue");
  StringValue barValue("barValue");

  TestingSettingsStorageFactory* testing_factory =
      new TestingSettingsStorageFactory();
  storage_factory_->Reset(testing_factory);

  SettingsStorage* good = AddExtensionAndGetStorage("good", type);
  SettingsStorage* bad = AddExtensionAndGetStorage("bad", type);

  good->Set("foo", fooValue);
  bad->Set("foo", fooValue);

  // good will successfully push foo:fooValue to sync, but bad will fail to
  // get them so won't.
  testing_factory->GetExisting("bad")->SetFailAllRequests(true);
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);
  testing_factory->GetExisting("bad")->SetFailAllRequests(false);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_.changes().size());

  // bad should now be disabled for sync.
  sync_.ClearChanges();
  good->Set("bar", barValue);
  bad->Set("bar", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(1u, sync_.changes().size());

  {
    SyncChangeList change_list;
    change_list.push_back(
        settings_sync_util::CreateUpdate("good", "foo", barValue));
    // (Sending ADD here even though it's updating, since that's what the state
    // of sync is.  In any case, it won't work.)
    change_list.push_back(
        settings_sync_util::CreateAdd("bad", "foo", barValue));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Re-enabling sync without failing should cause the local changes from bad
  // to be pushed to sync successfully, as should future changes to bad.
  sync_.ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("bad", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("bad", "bar").change_type());
  EXPECT_EQ(4u, sync_.changes().size());

  sync_.ClearChanges();
  good->Set("bar", fooValue);
  bad->Set("bar", fooValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(2u, sync_.changes().size());
}

TEST_F(ExtensionSettingsSyncTest, FailureToPushLocalStateDisablesSync) {
  syncable::ModelType model_type = syncable::EXTENSION_SETTINGS;
  Extension::Type type = Extension::TYPE_EXTENSION;

  StringValue fooValue("fooValue");
  StringValue barValue("barValue");

  TestingSettingsStorageFactory* testing_factory =
      new TestingSettingsStorageFactory();
  storage_factory_->Reset(testing_factory);

  SettingsStorage* good = AddExtensionAndGetStorage("good", type);
  SettingsStorage* bad = AddExtensionAndGetStorage("bad", type);

  // Only set bad; setting good will cause it to fail below.
  bad->Set("foo", fooValue);

  sync_.SetFailAllRequests(true);
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);
  sync_.SetFailAllRequests(false);

  // Changes from good will be send to sync, changes from bad won't.
  sync_.ClearChanges();
  good->Set("foo", barValue);
  bad->Set("foo", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_.changes().size());

  // Changes from sync will be sent to good, not to bad.
  {
    SyncChangeList change_list;
    change_list.push_back(
        settings_sync_util::CreateAdd("good", "bar", barValue));
    change_list.push_back(
        settings_sync_util::CreateAdd("bad", "bar", barValue));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Restarting sync makes everything work again.
  sync_.ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("bad", "foo").change_type());
  EXPECT_EQ(3u, sync_.changes().size());

  sync_.ClearChanges();
  good->Set("foo", fooValue);
  bad->Set("foo", fooValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(2u, sync_.changes().size());
}

TEST_F(ExtensionSettingsSyncTest, FailureToPushLocalChangeDisablesSync) {
  syncable::ModelType model_type = syncable::EXTENSION_SETTINGS;
  Extension::Type type = Extension::TYPE_EXTENSION;

  StringValue fooValue("fooValue");
  StringValue barValue("barValue");

  TestingSettingsStorageFactory* testing_factory =
      new TestingSettingsStorageFactory();
  storage_factory_->Reset(testing_factory);

  SettingsStorage* good = AddExtensionAndGetStorage("good", type);
  SettingsStorage* bad = AddExtensionAndGetStorage("bad", type);

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);

  // bad will fail to send changes.
  good->Set("foo", fooValue);
  sync_.SetFailAllRequests(true);
  bad->Set("foo", fooValue);
  sync_.SetFailAllRequests(false);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_.changes().size());

  // No further changes should be sent from bad.
  sync_.ClearChanges();
  good->Set("foo", barValue);
  bad->Set("foo", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_.changes().size());

  // Changes from sync will be sent to good, not to bad.
  {
    SyncChangeList change_list;
    change_list.push_back(
        settings_sync_util::CreateAdd("good", "bar", barValue));
    change_list.push_back(
        settings_sync_util::CreateAdd("bad", "bar", barValue));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Restarting sync makes everything work again.
  sync_.ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, SyncDataList(), &sync_);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_.GetOnlyChange("bad", "foo").change_type());
  EXPECT_EQ(3u, sync_.changes().size());

  sync_.ClearChanges();
  good->Set("foo", fooValue);
  bad->Set("foo", fooValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_.GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(2u, sync_.changes().size());
}

}  // namespace extensions

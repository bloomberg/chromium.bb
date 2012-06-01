// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/extensions/settings/failing_settings_storage.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/extensions/settings/settings_storage_factory.h"
#include "chrome/browser/extensions/settings/settings_sync_util.h"
#include "chrome/browser/extensions/settings/settings_test_util.h"
#include "chrome/browser/extensions/settings/syncable_settings_storage.h"
#include "chrome/browser/extensions/settings/testing_settings_storage.h"
#include "content/test/test_browser_thread.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"

using content::BrowserThread;

namespace extensions {

namespace util = settings_test_util;

namespace {

// To save typing SettingsStorage::DEFAULTS everywhere.
const SettingsStorage::WriteOptions DEFAULTS = SettingsStorage::DEFAULTS;

// Gets the pretty-printed JSON for a value.
static std::string GetJson(const Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(&value,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
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
          SyncChange::ACTION_INVALID, "", "",
          scoped_ptr<Value>(new DictionaryValue()));
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

class SyncChangeProcessorDelegate : public SyncChangeProcessor {
 public:
  explicit SyncChangeProcessorDelegate(SyncChangeProcessor* recipient)
      : recipient_(recipient) {
    DCHECK(recipient_);
  }
  virtual ~SyncChangeProcessorDelegate() {}

  // SyncChangeProcessor implementation.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE {
    return recipient_->ProcessSyncChanges(from_here, change_list);
  }

 private:
  // The recipient of all sync changes.
  SyncChangeProcessor* recipient_;

  DISALLOW_COPY_AND_ASSIGN(SyncChangeProcessorDelegate);
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
  // SettingsStorageFactory is refcounted.
  virtual ~TestingSettingsStorageFactory() {}

  // None of these storage areas are owned by this factory, so care must be
  // taken when calling GetExisting.
  std::map<std::string, TestingSettingsStorage*> created_;
};

void AssignSettingsService(SyncableService** dst,
                           const SettingsFrontend* frontend,
                           syncable::ModelType type) {
  *dst = frontend->GetBackendForSync(type);
}

}  // namespace

class ExtensionSettingsSyncTest : public testing::Test {
 public:
  ExtensionSettingsSyncTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()),
        storage_factory_(new util::ScopedSettingsStorageFactory()),
        sync_processor_(new MockSyncChangeProcessor),
        sync_processor_delegate_(new SyncChangeProcessorDelegate(
            sync_processor_.get())) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new util::MockProfile(temp_dir_.path()));
    storage_factory_->Reset(new SettingsLeveldbStorage::Factory());
    frontend_.reset(
        SettingsFrontend::Create(storage_factory_.get(), profile_.get()));
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
    return util::GetStorage(id, frontend_.get());
  }

  // Gets the SyncableService for the given sync type.
  SyncableService* GetSyncableService(syncable::ModelType model_type) {
    MessageLoop::current()->RunAllPending();
    return frontend_->GetBackendForSync(model_type);
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
  scoped_ptr<util::MockProfile> profile_;
  scoped_ptr<SettingsFrontend> frontend_;
  scoped_refptr<util::ScopedSettingsStorageFactory> storage_factory_;
  scoped_ptr<MockSyncChangeProcessor> sync_processor_;
  scoped_ptr<SyncChangeProcessorDelegate> sync_processor_delegate_;
};

// Get a semblance of coverage for both EXTENSION_SETTINGS and APP_SETTINGS
// sync by roughly alternative which one to test.

TEST_F(ExtensionSettingsSyncTest, NoDataDoesNotInvokeSync) {
  syncable::ModelType model_type = syncable::EXTENSION_SETTINGS;
  Extension::Type type = Extension::TYPE_EXTENSION;

  EXPECT_EQ(0u, GetAllSyncData(model_type).size());

  // Have one extension created before sync is set up, the other created after.
  AddExtensionAndGetStorage("s1", type);
  EXPECT_EQ(0u, GetAllSyncData(model_type).size());

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  AddExtensionAndGetStorage("s2", type);
  EXPECT_EQ(0u, GetAllSyncData(model_type).size());

  GetSyncableService(model_type)->StopSyncing(model_type);

  EXPECT_EQ(0u, sync_processor_->changes().size());
  EXPECT_EQ(0u, GetAllSyncData(model_type).size());
}

TEST_F(ExtensionSettingsSyncTest, InSyncDataDoesNotInvokeSync) {
  syncable::ModelType model_type = syncable::APP_SETTINGS;
  Extension::Type type = Extension::TYPE_PACKAGED_APP;

  StringValue value1("fooValue");
  ListValue value2;
  value2.Append(StringValue::CreateStringValue("barValue"));

  SettingsStorage* storage1 = AddExtensionAndGetStorage("s1", type);
  SettingsStorage* storage2 = AddExtensionAndGetStorage("s2", type);

  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "bar", value2);

  std::map<std::string, SettingSyncDataList> all_sync_data =
      GetAllSyncData(model_type);
  EXPECT_EQ(2u, all_sync_data.size());
  EXPECT_EQ(1u, all_sync_data["s1"].size());
  EXPECT_PRED_FORMAT2(ValuesEq, &value1, &all_sync_data["s1"][0].value());
  EXPECT_EQ(1u, all_sync_data["s2"].size());
  EXPECT_PRED_FORMAT2(ValuesEq, &value2, &all_sync_data["s2"][0].value());

  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s1", "foo", value1, model_type));
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2, model_type));

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, sync_data,
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  // Already in sync, so no changes.
  EXPECT_EQ(0u, sync_processor_->changes().size());

  // Regression test: not-changing the synced value shouldn't result in a sync
  // change, and changing the synced value should result in an update.
  storage1->Set(DEFAULTS, "foo", value1);
  EXPECT_EQ(0u, sync_processor_->changes().size());

  storage1->Set(DEFAULTS, "foo", value2);
  EXPECT_EQ(1u, sync_processor_->changes().size());
  SettingSyncData change = sync_processor_->GetOnlyChange("s1", "foo");
  EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
  EXPECT_TRUE(value2.Equals(&change.value()));

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

  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "bar", value2);

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  // All settings should have been pushed to sync.
  EXPECT_EQ(2u, sync_processor_->changes().size());
  SettingSyncData change = sync_processor_->GetOnlyChange("s1", "foo");
  EXPECT_EQ(SyncChange::ACTION_ADD, change.change_type());
  EXPECT_TRUE(value1.Equals(&change.value()));
  change = sync_processor_->GetOnlyChange("s2", "bar");
  EXPECT_EQ(SyncChange::ACTION_ADD, change.change_type());
  EXPECT_TRUE(value2.Equals(&change.value()));

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
  storage1->Set(DEFAULTS, "overwriteMe", value1);

  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s1", "foo", value1, model_type));
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2, model_type));
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, sync_data,
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));
  expected1.Set("foo", value1.DeepCopy());
  expected2.Set("bar", value2.DeepCopy());

  SettingsStorage* storage2 = AddExtensionAndGetStorage("s2", type);

  // All changes should be local, so no sync changes.
  EXPECT_EQ(0u, sync_processor_->changes().size());

  // Sync settings should have been pushed to local settings.
  EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

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

  storage1->Set(DEFAULTS, "foo", value1);
  expected1.Set("foo", value1.DeepCopy());

  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2, model_type));

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, sync_data,
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));
  expected2.Set("bar", value2.DeepCopy());

  // Make sync add some settings.
  SyncChangeList change_list;
  change_list.push_back(settings_sync_util::CreateAdd(
      "s1", "bar", value2, model_type));
  change_list.push_back(settings_sync_util::CreateAdd(
      "s2", "foo", value1, model_type));
  GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Set("bar", value2.DeepCopy());
  expected2.Set("foo", value1.DeepCopy());

  EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  // Make sync update some settings, storage1 the new setting, storage2 the
  // initial setting.
  change_list.clear();
  change_list.push_back(settings_sync_util::CreateUpdate(
      "s1", "bar", value2, model_type));
  change_list.push_back(settings_sync_util::CreateUpdate(
      "s2", "bar", value1, model_type));
  GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Set("bar", value2.DeepCopy());
  expected2.Set("bar", value1.DeepCopy());

  EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  // Make sync remove some settings, storage1 the initial setting, storage2 the
  // new setting.
  change_list.clear();
  change_list.push_back(settings_sync_util::CreateDelete(
      "s1", "foo", model_type));
  change_list.push_back(settings_sync_util::CreateDelete(
      "s2", "foo", model_type));
  GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Remove("foo", NULL);
  expected2.Remove("foo", NULL);

  EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

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

  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "foo", value1);

  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s3", "bar", value2, model_type));
  sync_data.push_back(settings_sync_util::CreateData(
      "s4", "bar", value2, model_type));

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type, sync_data,
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  // Add something locally.
  storage1->Set(DEFAULTS, "bar", value2);
  storage2->Set(DEFAULTS, "bar", value2);
  storage3->Set(DEFAULTS, "foo", value1);
  storage4->Set(DEFAULTS, "foo", value1);

  SettingSyncData change = sync_processor_->GetOnlyChange("s1", "bar");
    EXPECT_EQ(SyncChange::ACTION_ADD, change.change_type());
    EXPECT_TRUE(value2.Equals(&change.value()));
  sync_processor_->GetOnlyChange("s2", "bar");
    EXPECT_EQ(SyncChange::ACTION_ADD, change.change_type());
    EXPECT_TRUE(value2.Equals(&change.value()));
  change = sync_processor_->GetOnlyChange("s3", "foo");
    EXPECT_EQ(SyncChange::ACTION_ADD, change.change_type());
    EXPECT_TRUE(value1.Equals(&change.value()));
  change = sync_processor_->GetOnlyChange("s4", "foo");
    EXPECT_EQ(SyncChange::ACTION_ADD, change.change_type());
    EXPECT_TRUE(value1.Equals(&change.value()));

  // Change something locally, storage1/3 the new setting and storage2/4 the
  // initial setting, for all combinations of local vs sync intialisation and
  // new vs initial.
  sync_processor_->ClearChanges();
  storage1->Set(DEFAULTS, "bar", value1);
  storage2->Set(DEFAULTS, "foo", value2);
  storage3->Set(DEFAULTS, "bar", value1);
  storage4->Set(DEFAULTS, "foo", value2);

  change = sync_processor_->GetOnlyChange("s1", "bar");
    EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    EXPECT_TRUE(value1.Equals(&change.value()));
  change = sync_processor_->GetOnlyChange("s2", "foo");
    EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    EXPECT_TRUE(value2.Equals(&change.value()));
  change = sync_processor_->GetOnlyChange("s3", "bar");
    EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    EXPECT_TRUE(value1.Equals(&change.value()));
  change = sync_processor_->GetOnlyChange("s4", "foo");
    EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    EXPECT_TRUE(value2.Equals(&change.value()));

  // Remove something locally, storage1/3 the new setting and storage2/4 the
  // initial setting, for all combinations of local vs sync intialisation and
  // new vs initial.
  sync_processor_->ClearChanges();
  storage1->Remove("foo");
  storage2->Remove("bar");
  storage3->Remove("foo");
  storage4->Remove("bar");

  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s1", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s2", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s3", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s4", "bar").change_type());

  // Remove some nonexistent settings.
  sync_processor_->ClearChanges();
  storage1->Remove("foo");
  storage2->Remove("bar");
  storage3->Remove("foo");
  storage4->Remove("bar");

  EXPECT_EQ(0u, sync_processor_->changes().size());

  // Clear the rest of the settings.  Add the removed ones back first so that
  // more than one setting is cleared.
  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "bar", value2);
  storage3->Set(DEFAULTS, "foo", value1);
  storage4->Set(DEFAULTS, "bar", value2);

  sync_processor_->ClearChanges();
  storage1->Clear();
  storage2->Clear();
  storage3->Clear();
  storage4->Clear();

  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s1", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s1", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s2", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s2", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s3", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s3", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s4", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_DELETE,
      sync_processor_->GetOnlyChange("s4", "bar").change_type());

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

  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "bar", value2);

  std::map<std::string, SettingSyncDataList> extension_sync_data =
      GetAllSyncData(syncable::EXTENSION_SETTINGS);
  EXPECT_EQ(1u, extension_sync_data.size());
  EXPECT_EQ(1u, extension_sync_data["s1"].size());
  EXPECT_PRED_FORMAT2(ValuesEq, &value1, &extension_sync_data["s1"][0].value());

  std::map<std::string, SettingSyncDataList> app_sync_data =
      GetAllSyncData(syncable::APP_SETTINGS);
  EXPECT_EQ(1u, app_sync_data.size());
  EXPECT_EQ(1u, app_sync_data["s2"].size());
  EXPECT_PRED_FORMAT2(ValuesEq, &value2, &app_sync_data["s2"][0].value());

  // Stop each separately, there should be no changes either time.
  SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s1", "foo", value1, syncable::EXTENSION_SETTINGS));

  GetSyncableService(syncable::EXTENSION_SETTINGS)->MergeDataAndStartSyncing(
      syncable::EXTENSION_SETTINGS,
      sync_data,
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));
  GetSyncableService(syncable::EXTENSION_SETTINGS)->
      StopSyncing(syncable::EXTENSION_SETTINGS);
  EXPECT_EQ(0u, sync_processor_->changes().size());

  sync_data.clear();
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2, syncable::APP_SETTINGS));

  scoped_ptr<SyncChangeProcessorDelegate> app_settings_delegate_(
      new SyncChangeProcessorDelegate(sync_processor_.get()));
  GetSyncableService(syncable::APP_SETTINGS)->MergeDataAndStartSyncing(
      syncable::APP_SETTINGS,
      sync_data,
      app_settings_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));
  GetSyncableService(syncable::APP_SETTINGS)->
      StopSyncing(syncable::APP_SETTINGS);
  EXPECT_EQ(0u, sync_processor_->changes().size());
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
    sync_data.push_back(settings_sync_util::CreateData(
          "good", "foo", fooValue, model_type));
    sync_data.push_back(settings_sync_util::CreateData(
          "bad", "foo", fooValue, model_type));
    GetSyncableService(model_type)->MergeDataAndStartSyncing(
        model_type,
        sync_data,
        sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
        scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));
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
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", barValue);
  bad->Set(DEFAULTS, "bar", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

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
    change_list.push_back(settings_sync_util::CreateUpdate(
          "good", "foo", barValue, model_type));
    // (Sending UPDATE here even though it's adding, since that's what the state
    // of sync is.  In any case, it won't work.)
    change_list.push_back(settings_sync_util::CreateUpdate(
          "bad", "foo", barValue, model_type));
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
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", fooValue);
  bad->Set(DEFAULTS, "bar", fooValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

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
    change_list.push_back(settings_sync_util::CreateUpdate(
          "good", "foo", fooValue, model_type));
    // (Ditto.)
    change_list.push_back(settings_sync_util::CreateUpdate(
          "bad", "foo", fooValue, model_type));
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
  sync_processor_->ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  sync_processor_delegate_.reset(new SyncChangeProcessorDelegate(
      sync_processor_.get()));
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  // Local settings will have been pushed to sync, since it's empty (in this
  // test; presumably it wouldn't be live, since we've been getting changes).
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("bad", "bar").change_type());
  EXPECT_EQ(3u, sync_processor_->changes().size());

  // Live local changes now get pushed, too.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", barValue);
  bad->Set(DEFAULTS, "bar", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("bad", "bar").change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());

  // And ProcessSyncChanges work, too.
  {
    SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateUpdate(
          "good", "bar", fooValue, model_type));
    change_list.push_back(settings_sync_util::CreateUpdate(
          "bad", "bar", fooValue, model_type));
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
    sync_data.push_back(settings_sync_util::CreateData(
          "good", "foo", fooValue, model_type));
    sync_data.push_back(settings_sync_util::CreateData(
          "bad", "foo", fooValue, model_type));
    GetSyncableService(model_type)->MergeDataAndStartSyncing(
        model_type,
        sync_data,
        sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
        scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));
  }

  EXPECT_EQ(0u, sync_processor_->changes().size());

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
    change_list.push_back(settings_sync_util::CreateAdd(
          "good", "bar", barValue, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "bar", barValue, model_type));
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
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", barValue);
  bad->Set(DEFAULTS, "foo", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // No more changes received from sync should go to bad.
  {
    SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateAdd(
          "good", "foo", fooValue, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "foo", fooValue, model_type));
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

  good->Set(DEFAULTS, "foo", fooValue);
  bad->Set(DEFAULTS, "foo", fooValue);

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
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("bad", "foo").change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());

  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", barValue);
  bad->Set(DEFAULTS, "bar", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("bad", "bar").change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());
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

  good->Set(DEFAULTS, "foo", fooValue);
  bad->Set(DEFAULTS, "foo", fooValue);

  // good will successfully push foo:fooValue to sync, but bad will fail to
  // get them so won't.
  testing_factory->GetExisting("bad")->SetFailAllRequests(true);
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));
  testing_factory->GetExisting("bad")->SetFailAllRequests(false);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // bad should now be disabled for sync.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", barValue);
  bad->Set(DEFAULTS, "bar", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  {
    SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateUpdate(
          "good", "foo", barValue, model_type));
    // (Sending ADD here even though it's updating, since that's what the state
    // of sync is.  In any case, it won't work.)
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "foo", barValue, model_type));
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
  sync_processor_->ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  sync_processor_delegate_.reset(new SyncChangeProcessorDelegate(
      sync_processor_.get()));
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("bad", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("bad", "bar").change_type());
  EXPECT_EQ(4u, sync_processor_->changes().size());

  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", fooValue);
  bad->Set(DEFAULTS, "bar", fooValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());
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
  bad->Set(DEFAULTS, "foo", fooValue);

  sync_processor_->SetFailAllRequests(true);
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));
  sync_processor_->SetFailAllRequests(false);

  // Changes from good will be send to sync, changes from bad won't.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", barValue);
  bad->Set(DEFAULTS, "foo", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // Changes from sync will be sent to good, not to bad.
  {
    SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateAdd(
          "good", "bar", barValue, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "bar", barValue, model_type));
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
  sync_processor_->ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  sync_processor_delegate_.reset(new SyncChangeProcessorDelegate(
      sync_processor_.get()));
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("bad", "foo").change_type());
  EXPECT_EQ(3u, sync_processor_->changes().size());

  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", fooValue);
  bad->Set(DEFAULTS, "foo", fooValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());
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
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  // bad will fail to send changes.
  good->Set(DEFAULTS, "foo", fooValue);
  sync_processor_->SetFailAllRequests(true);
  bad->Set(DEFAULTS, "foo", fooValue);
  sync_processor_->SetFailAllRequests(false);

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // No further changes should be sent from bad.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", barValue);
  bad->Set(DEFAULTS, "foo", barValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // Changes from sync will be sent to good, not to bad.
  {
    SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateAdd(
          "good", "bar", barValue, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "bar", barValue, model_type));
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
  sync_processor_->ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  sync_processor_delegate_.reset(new SyncChangeProcessorDelegate(
      sync_processor_.get()));
  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("good", "bar").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_ADD,
      sync_processor_->GetOnlyChange("bad", "foo").change_type());
  EXPECT_EQ(3u, sync_processor_->changes().size());

  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", fooValue);
  bad->Set(DEFAULTS, "foo", fooValue);

  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(
      SyncChange::ACTION_UPDATE,
      sync_processor_->GetOnlyChange("good", "foo").change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());
}

TEST_F(ExtensionSettingsSyncTest,
       LargeOutgoingChangeRejectedButIncomingAccepted) {
  syncable::ModelType model_type = syncable::APP_SETTINGS;
  Extension::Type type = Extension::TYPE_PACKAGED_APP;

  // This value should be larger than the limit in settings_backend.cc.
  std::string string_5k;
  for (size_t i = 0; i < 5000; ++i) {
    string_5k.append("a");
  }
  StringValue large_value(string_5k);

  GetSyncableService(model_type)->MergeDataAndStartSyncing(
      model_type,
      SyncDataList(),
      sync_processor_delegate_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  // Large local change rejected and doesn't get sent out.
  SettingsStorage* storage1 = AddExtensionAndGetStorage("s1", type);
  EXPECT_TRUE(storage1->Set(DEFAULTS, "large_value", large_value).HasError());
  EXPECT_EQ(0u, sync_processor_->changes().size());

  // Large incoming change should still get accepted.
  SettingsStorage* storage2 = AddExtensionAndGetStorage("s2", type);
  {
    SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateAdd(
          "s1", "large_value", large_value, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "s2", "large_value", large_value, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }
  {
    DictionaryValue expected;
    expected.Set("large_value", large_value.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, expected, storage1->Get());
    EXPECT_PRED_FORMAT2(SettingsEq, expected, storage2->Get());
  }

  GetSyncableService(model_type)->StopSyncing(model_type);
}

}  // namespace extensions

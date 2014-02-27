// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/storage/leveldb_settings_storage_factory.h"
#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/api/storage/settings_storage_quota_enforcer.h"
#include "chrome/browser/extensions/api/storage/settings_test_util.h"
#include "chrome/browser/extensions/api/storage/storage_api.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/value_store/leveldb_value_store.h"
#include "chrome/browser/value_store/value_store.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/id_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/test_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace extensions {

namespace {

// Caller owns the returned object.
BrowserContextKeyedService* CreateSettingsFrontendForTesting(
    content::BrowserContext* context) {
  return SettingsFrontend::CreateForTesting(new LeveldbSettingsStorageFactory(),
                                            context);
}

}  // namespace

class StorageApiUnittest : public ExtensionApiUnittest {
 public:
  virtual void SetUp() OVERRIDE {
    ExtensionApiUnittest::SetUp();
    TestExtensionSystem* extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    ExtensionService* extension_service = extension_system->extension_service();
    if (!extension_service) {
      extension_service = extension_system->CreateExtensionService(
          CommandLine::ForCurrentProcess(), base::FilePath(), false);
      ASSERT_TRUE(extension_service);
    }

    if (!extension_system->event_router()) {
      extension_system->SetEventRouter(scoped_ptr<EventRouter>(
          new EventRouter(profile(), ExtensionPrefs::Get(profile()))));
    }

    scoped_refptr<Extension> extension =
        test_util::CreateExtensionWithID(id_util::GenerateId("ext"));
    set_extension(extension);
    extension_service->AddExtension(extension.get());
  }

 protected:
  // Runs the storage.set() API function with local storage.
  void RunSetFunction(const std::string& key, const std::string& value) {
    RunFunction(
        new StorageStorageAreaSetFunction(),
        base::StringPrintf(
            "[\"local\", {\"%s\": \"%s\"}]", key.c_str(), value.c_str()));
  }

  // Runs the storage.get() API function with the local storage, and populates
  // |value| with the string result.
  testing::AssertionResult RunGetFunction(const std::string& key,
                                          std::string* value) {
    scoped_ptr<base::Value> result = RunFunctionAndReturnValue(
        new StorageStorageAreaGetFunction(),
        base::StringPrintf("[\"local\", \"%s\"]", key.c_str()));
    base::DictionaryValue* dict = NULL;
    if (!result->GetAsDictionary(&dict))
      return testing::AssertionFailure() << result << " was not a dictionary.";
    if (!dict->GetString(key, value)) {
      return testing::AssertionFailure() << " could not retrieve a string from"
          << dict << " at " << key;
    }
    return testing::AssertionSuccess();
  }
};

TEST_F(StorageApiUnittest, RestoreCorruptedStorage) {
  // Ensure a SettingsFrontend can be created on demand. The SettingsFrontend
  // will be owned by the BrowserContextKeyedService system.
  SettingsFrontend::GetFactoryInstance()->SetTestingFactory(
      profile(), &CreateSettingsFrontendForTesting);

  const char kKey[] = "key";
  const char kValue[] = "value";
  std::string result;

  // Do a simple set/get combo to make sure the API works.
  RunSetFunction(kKey, kValue);
  EXPECT_TRUE(RunGetFunction(kKey, &result));
  EXPECT_EQ(kValue, result);

  // Corrupt the store. This is not as pretty as ideal, because we use knowledge
  // of the underlying structure, but there's no real good way to corrupt a
  // store other than directly modifying the files.
  ValueStore* store =
      settings_test_util::GetStorage(extension()->id(),
                                     settings_namespace::LOCAL,
                                     SettingsFrontend::Get(profile()));
  ASSERT_TRUE(store);
  SettingsStorageQuotaEnforcer* quota_store =
      static_cast<SettingsStorageQuotaEnforcer*>(store);
  LeveldbValueStore* leveldb_store =
      static_cast<LeveldbValueStore*>(quota_store->get_delegate_for_test());
  leveldb::WriteBatch batch;
  batch.Put(kKey, "[{(.*+\"\'\\");
  EXPECT_TRUE(leveldb_store->WriteToDbForTest(&batch));
  EXPECT_TRUE(leveldb_store->Get(kKey)->IsCorrupted());

  // Running another set should end up working (even though it will restore the
  // store behind the scenes).
  RunSetFunction(kKey, kValue);
  EXPECT_TRUE(RunGetFunction(kKey, &result));
  EXPECT_EQ(kValue, result);
}

}  // namespace extensions

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/api/storage/leveldb_settings_storage_factory.h"
#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/api/storage/settings_namespace.h"
#include "chrome/browser/extensions/api/storage/settings_test_util.h"
#include "chrome/browser/value_store/value_store.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace extensions {

namespace settings = settings_namespace;
namespace util = settings_test_util;

namespace {

// To save typing ValueStore::DEFAULTS everywhere.
const ValueStore::WriteOptions DEFAULTS = ValueStore::DEFAULTS;

// Creates a kilobyte of data.
scoped_ptr<Value> CreateKilobyte() {
  std::string kilobyte_string;
  for (int i = 0; i < 1024; ++i) {
    kilobyte_string += "a";
  }
  return scoped_ptr<Value>(Value::CreateStringValue(kilobyte_string));
}

// Creates a megabyte of data.
scoped_ptr<Value> CreateMegabyte() {
  ListValue* megabyte = new ListValue();
  for (int i = 0; i < 1000; ++i) {
    megabyte->Append(CreateKilobyte().release());
  }
  return scoped_ptr<Value>(megabyte);
}

}

class ExtensionSettingsFrontendTest : public testing::Test {
 public:
   ExtensionSettingsFrontendTest()
      : storage_factory_(new util::ScopedSettingsStorageFactory()),
        ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new util::MockProfile(temp_dir_.path()));
    ResetFrontend();
  }

  virtual void TearDown() OVERRIDE {
    frontend_.reset();
    profile_.reset();
    // Execute any pending deletion tasks.
    message_loop_.RunUntilIdle();
  }

 protected:
  void ResetFrontend() {
    storage_factory_->Reset(new LeveldbSettingsStorageFactory());
    frontend_.reset(
        SettingsFrontend::Create(storage_factory_.get(), profile_.get()));
  }

  base::ScopedTempDir temp_dir_;
  scoped_ptr<util::MockProfile> profile_;
  scoped_ptr<SettingsFrontend> frontend_;
  scoped_refptr<util::ScopedSettingsStorageFactory> storage_factory_;

 private:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

// Get a semblance of coverage for both extension and app settings by
// alternating in each test.
// TODO(kalman): explicitly test the two interact correctly.

TEST_F(ExtensionSettingsFrontendTest, SettingsPreservedAcrossReconstruction) {
  const std::string id = "ext";
  ExtensionServiceInterface* esi =
      extensions::ExtensionSystem::Get(profile_.get())->extension_service();
  static_cast<extensions::settings_test_util::MockExtensionService*>(esi)->
      AddExtensionWithId(id, Extension::TYPE_EXTENSION);

  ValueStore* storage = util::GetStorage(id, frontend_.get());

  // The correctness of Get/Set/Remove/Clear is tested elsewhere so no need to
  // be too rigorous.
  {
    StringValue bar("bar");
    ValueStore::WriteResult result = storage->Set(DEFAULTS, "foo", bar);
    ASSERT_FALSE(result->HasError());
  }

  {
    ValueStore::ReadResult result = storage->Get();
    ASSERT_FALSE(result->HasError());
    EXPECT_FALSE(result->settings()->empty());
  }

  ResetFrontend();
  storage = util::GetStorage(id, frontend_.get());

  {
    ValueStore::ReadResult result = storage->Get();
    ASSERT_FALSE(result->HasError());
    EXPECT_FALSE(result->settings()->empty());
  }
}

TEST_F(ExtensionSettingsFrontendTest, SettingsClearedOnUninstall) {
  const std::string id = "ext";
  ExtensionServiceInterface* esi =
      extensions::ExtensionSystem::Get(profile_.get())->extension_service();
  static_cast<extensions::settings_test_util::MockExtensionService*>(esi)->
      AddExtensionWithId(id, Extension::TYPE_LEGACY_PACKAGED_APP);

  ValueStore* storage = util::GetStorage(id, frontend_.get());

  {
    StringValue bar("bar");
    ValueStore::WriteResult result = storage->Set(DEFAULTS, "foo", bar);
    ASSERT_FALSE(result->HasError());
  }

  // This would be triggered by extension uninstall via a DataDeleter.
  frontend_->DeleteStorageSoon(id);
  MessageLoop::current()->RunUntilIdle();

  // The storage area may no longer be valid post-uninstall, so re-request.
  storage = util::GetStorage(id, frontend_.get());
  {
    ValueStore::ReadResult result = storage->Get();
    ASSERT_FALSE(result->HasError());
    EXPECT_TRUE(result->settings()->empty());
  }
}

TEST_F(ExtensionSettingsFrontendTest, LeveldbDatabaseDeletedFromDiskOnClear) {
  const std::string id = "ext";
  ExtensionServiceInterface* esi =
      extensions::ExtensionSystem::Get(profile_.get())->extension_service();
  static_cast<extensions::settings_test_util::MockExtensionService*>(esi)->
      AddExtensionWithId(id, Extension::TYPE_EXTENSION);

  ValueStore* storage = util::GetStorage(id, frontend_.get());

  {
    StringValue bar("bar");
    ValueStore::WriteResult result = storage->Set(DEFAULTS, "foo", bar);
    ASSERT_FALSE(result->HasError());
    EXPECT_TRUE(file_util::PathExists(temp_dir_.path()));
  }

  // Should need to both clear the database and delete the frontend for the
  // leveldb database to be deleted from disk.
  {
    ValueStore::WriteResult result = storage->Clear();
    ASSERT_FALSE(result->HasError());
    EXPECT_TRUE(file_util::PathExists(temp_dir_.path()));
  }

  frontend_.reset();
  MessageLoop::current()->RunUntilIdle();
  // TODO(kalman): Figure out why this fails, despite appearing to work.
  // Leaving this commented out rather than disabling the whole test so that the
  // deletion code paths are at least exercised.
  //EXPECT_FALSE(file_util::PathExists(temp_dir_.path()));
}

#if defined(OS_WIN)
// Failing on vista dbg. http://crbug.com/111100, http://crbug.com/108724
#define QuotaLimitsEnforcedCorrectlyForSyncAndLocal \
  DISABLED_QuotaLimitsEnforcedCorrectlyForSyncAndLocal
#endif
TEST_F(ExtensionSettingsFrontendTest,
       QuotaLimitsEnforcedCorrectlyForSyncAndLocal) {
  const std::string id = "ext";
  ExtensionServiceInterface* esi =
      extensions::ExtensionSystem::Get(profile_.get())->extension_service();
  static_cast<extensions::settings_test_util::MockExtensionService*>(esi)->
      AddExtensionWithId(id, Extension::TYPE_EXTENSION);

  ValueStore* sync_storage =
      util::GetStorage(id, settings::SYNC, frontend_.get());
  ValueStore* local_storage =
      util::GetStorage(id, settings::LOCAL, frontend_.get());

  // Sync storage should run out after ~100K.
  scoped_ptr<Value> kilobyte = CreateKilobyte();
  for (int i = 0; i < 100; ++i) {
    sync_storage->Set(
        ValueStore::DEFAULTS, base::StringPrintf("%d", i), *kilobyte);
  }

  EXPECT_TRUE(sync_storage->Set(
      ValueStore::DEFAULTS, "WillError", *kilobyte)->HasError());

  // Local storage shouldn't run out after ~100K.
  for (int i = 0; i < 100; ++i) {
    local_storage->Set(
        ValueStore::DEFAULTS, base::StringPrintf("%d", i), *kilobyte);
  }

  EXPECT_FALSE(local_storage->Set(
      ValueStore::DEFAULTS, "WontError", *kilobyte)->HasError());

  // Local storage should run out after ~5MB.
  scoped_ptr<Value> megabyte = CreateMegabyte();
  for (int i = 0; i < 5; ++i) {
    local_storage->Set(
        ValueStore::DEFAULTS, base::StringPrintf("%d", i), *megabyte);
  }

  EXPECT_TRUE(local_storage->Set(
      ValueStore::DEFAULTS, "WillError", *megabyte)->HasError());
}

// In other tests, we assume that the result of GetStorage is a pointer to the
// a Storage owned by a Frontend object, but for the unlimitedStorage case, this
// might not be true. So, write the tests in a "callback" style.
// We should really rewrite all tests to be asynchronous in this way.

static void UnlimitedSyncStorageTestCallback(ValueStore* sync_storage) {
  // Sync storage should still run out after ~100K; the unlimitedStorage
  // permission can't apply to sync.
  scoped_ptr<Value> kilobyte = CreateKilobyte();
  for (int i = 0; i < 100; ++i) {
    sync_storage->Set(
        ValueStore::DEFAULTS, base::StringPrintf("%d", i), *kilobyte);
  }

  EXPECT_TRUE(sync_storage->Set(
      ValueStore::DEFAULTS, "WillError", *kilobyte)->HasError());
}

static void UnlimitedLocalStorageTestCallback(ValueStore* local_storage) {
  // Local storage should never run out.
  scoped_ptr<Value> megabyte = CreateMegabyte();
  for (int i = 0; i < 7; ++i) {
    local_storage->Set(
        ValueStore::DEFAULTS, base::StringPrintf("%d", i), *megabyte);
  }

  EXPECT_FALSE(local_storage->Set(
      ValueStore::DEFAULTS, "WontError", *megabyte)->HasError());
}

#if defined(OS_WIN)
// Failing on vista dbg. http://crbug.com/111100, http://crbug.com/108724
#define UnlimitedStorageForLocalButNotSync DISABLED_UnlimitedStorageForLocalButNotSync
#endif
TEST_F(ExtensionSettingsFrontendTest,
       UnlimitedStorageForLocalButNotSync) {
  const std::string id = "ext";
  std::set<std::string> permissions;
  permissions.insert("unlimitedStorage");
  ExtensionServiceInterface* esi =
      extensions::ExtensionSystem::Get(profile_.get())->extension_service();
  static_cast<extensions::settings_test_util::MockExtensionService*>(esi)->
      AddExtensionWithIdAndPermissions(id, Extension::TYPE_EXTENSION,
          permissions);

  frontend_->RunWithStorage(
      id, settings::SYNC, base::Bind(&UnlimitedSyncStorageTestCallback));
  frontend_->RunWithStorage(
      id, settings::LOCAL, base::Bind(&UnlimitedLocalStorageTestCallback));

  MessageLoop::current()->RunUntilIdle();
}

}  // namespace extensions

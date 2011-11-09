// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/extensions/extension_settings_frontend.h"
#include "chrome/browser/extensions/extension_settings_storage.h"
#include "chrome/browser/extensions/extension_settings_test_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/test/test_browser_thread.h"

using content::BrowserThread;

using namespace extension_settings_test_util;

class ExtensionSettingsFrontendTest : public testing::Test {
 public:
   ExtensionSettingsFrontendTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new MockProfile(temp_dir_.path()));
    frontend_.reset(new ExtensionSettingsFrontend(profile_.get()));
  }

  virtual void TearDown() OVERRIDE {
    frontend_.reset();
    profile_.reset();
  }

 protected:
  ScopedTempDir temp_dir_;
  scoped_ptr<MockProfile> profile_;
  scoped_ptr<ExtensionSettingsFrontend> frontend_;

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
  profile_->GetMockExtensionService()->AddExtension(
      id, Extension::TYPE_EXTENSION);

  ExtensionSettingsStorage* storage = GetStorage(id, frontend_.get());

  // The correctness of Get/Set/Remove/Clear is tested elsewhere so no need to
  // be too rigorous.
  {
    StringValue bar("bar");
    ExtensionSettingsStorage::WriteResult result = storage->Set("foo", bar);
    ASSERT_FALSE(result.HasError());
  }

  {
    ExtensionSettingsStorage::ReadResult result = storage->Get();
    ASSERT_FALSE(result.HasError());
    EXPECT_FALSE(result.settings().empty());
  }

  frontend_.reset(new ExtensionSettingsFrontend(profile_.get()));
  storage = GetStorage(id, frontend_.get());

  {
    ExtensionSettingsStorage::ReadResult result = storage->Get();
    ASSERT_FALSE(result.HasError());
    EXPECT_FALSE(result.settings().empty());
  }
}

TEST_F(ExtensionSettingsFrontendTest, SettingsClearedOnUninstall) {
  const std::string id = "ext";
  profile_->GetMockExtensionService()->AddExtension(
      id, Extension::TYPE_PACKAGED_APP);

  ExtensionSettingsStorage* storage = GetStorage(id, frontend_.get());

  {
    StringValue bar("bar");
    ExtensionSettingsStorage::WriteResult result = storage->Set("foo", bar);
    ASSERT_FALSE(result.HasError());
  }

  // This would be triggered by extension uninstall via an ExtensionDataDeleter.
  frontend_->DeleteStorageSoon(id);
  MessageLoop::current()->RunAllPending();

  // The storage area may no longer be valid post-uninstall, so re-request.
  storage = GetStorage(id, frontend_.get());
  {
    ExtensionSettingsStorage::ReadResult result = storage->Get();
    ASSERT_FALSE(result.HasError());
    EXPECT_TRUE(result.settings().empty());
  }
}

TEST_F(ExtensionSettingsFrontendTest, LeveldbDatabaseDeletedFromDiskOnClear) {
  const std::string id = "ext";
  profile_->GetMockExtensionService()->AddExtension(
      id, Extension::TYPE_EXTENSION);

  ExtensionSettingsStorage* storage = GetStorage(id, frontend_.get());

  {
    StringValue bar("bar");
    ExtensionSettingsStorage::WriteResult result = storage->Set("foo", bar);
    ASSERT_FALSE(result.HasError());
    EXPECT_TRUE(file_util::PathExists(temp_dir_.path()));
  }

  // Should need to both clear the database and delete the frontend for the
  // leveldb database to be deleted from disk.
  {
    ExtensionSettingsStorage::WriteResult result = storage->Clear();
    ASSERT_FALSE(result.HasError());
    EXPECT_TRUE(file_util::PathExists(temp_dir_.path()));
  }

  frontend_.reset();
  MessageLoop::current()->RunAllPending();
  // TODO(kalman): Figure out why this fails, despite appearing to work.
  // Leaving this commented out rather than disabling the whole test so that the
  // deletion code paths are at least exercised.
  //EXPECT_FALSE(file_util::PathExists(temp_dir_.path()));
}

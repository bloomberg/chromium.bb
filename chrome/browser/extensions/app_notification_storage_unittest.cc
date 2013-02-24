// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/app_notification.h"
#include "chrome/browser/extensions/app_notification_storage.h"
#include "chrome/browser/extensions/app_notification_test_util.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace util = app_notification_test_util;

namespace extensions {

class AppNotificationStorageTest : public testing::Test {
 public:
  AppNotificationStorageTest() :
      file_thread_(BrowserThread::FILE, &message_loop_) {}
  virtual ~AppNotificationStorageTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    storage_path_ = dir_.path().AppendASCII("App Notifications");
    storage_.reset(AppNotificationStorage::Create(storage_path_));
    ASSERT_TRUE(storage_.get() != NULL);
  }

 protected:
  // Returns whether the database file(s) exist on disk yet.
  bool DatabaseExistsOnDisk() {
    if (!storage_.get())
      return false;

    return file_util::PathExists(storage_path_);
  }

  MessageLoop message_loop_;
  content::TestBrowserThread file_thread_;
  base::ScopedTempDir dir_;
  base::FilePath storage_path_;
  scoped_ptr<AppNotificationStorage> storage_;
};

// Tests simple operations.
TEST_F(AppNotificationStorageTest, Basics) {
  std::set<std::string> tmp_ids;
  AppNotificationList tmp_list;
  std::string id1 = extension_test_util::MakeId("1");

  // Try operations on non-existent keys.
  EXPECT_TRUE(storage_->GetExtensionIds(&tmp_ids));
  EXPECT_TRUE(tmp_ids.empty());
  EXPECT_TRUE(storage_->Get(id1, &tmp_list));
  EXPECT_TRUE(tmp_list.empty());
  EXPECT_TRUE(storage_->Delete(id1));
  EXPECT_FALSE(DatabaseExistsOnDisk());

  // Add some items.
  AppNotificationList list;
  util::AddNotifications(&list, id1, 2, "whatever");
  EXPECT_TRUE(storage_->Set(id1, list));
  EXPECT_TRUE(DatabaseExistsOnDisk());

  // Try getting them back.
  tmp_list.clear();
  EXPECT_TRUE(storage_->Get(id1, &tmp_list));
  util::ExpectListsEqual(list, tmp_list);
  EXPECT_TRUE(storage_->GetExtensionIds(&tmp_ids));
  EXPECT_EQ(tmp_ids.size(), 1U);
  EXPECT_TRUE(ContainsKey(tmp_ids, id1));
}

// Tests with multiple extensions.
TEST_F(AppNotificationStorageTest, MultipleExtensions) {
  std::string id1 = extension_test_util::MakeId("1");
  std::string id2 = extension_test_util::MakeId("2");

  // Add items for id1.
  AppNotificationList list1;
  util::AddNotifications(&list1, id1, 2, "one");
  EXPECT_TRUE(storage_->Set(id1, list1));

  // Add items for id2.
  AppNotificationList list2;
  util::AddNotifications(&list2, id2, 3, "two");
  EXPECT_TRUE(storage_->Set(id2, list2));

  // Verify the items are present
  std::set<std::string> tmp_ids;
  EXPECT_TRUE(storage_->GetExtensionIds(&tmp_ids));
  EXPECT_EQ(tmp_ids.size(), 2U);
  EXPECT_TRUE(ContainsKey(tmp_ids, id1));
  EXPECT_TRUE(ContainsKey(tmp_ids, id2));

  AppNotificationList tmp_list;
  EXPECT_TRUE(storage_->Get(id1, &tmp_list));
  util::ExpectListsEqual(tmp_list, list1);
  tmp_list.clear();
  EXPECT_TRUE(storage_->Get(id2, &tmp_list));
  util::ExpectListsEqual(tmp_list, list2);

  // Delete the id1 items and check the results.
  EXPECT_TRUE(storage_->Delete(id1));
  tmp_ids.clear();
  EXPECT_TRUE(storage_->GetExtensionIds(&tmp_ids));
  EXPECT_EQ(tmp_ids.size(), 1U);
  EXPECT_FALSE(ContainsKey(tmp_ids, id1));
  EXPECT_TRUE(ContainsKey(tmp_ids, id2));
  tmp_list.clear();
  EXPECT_TRUE(storage_->Get(id1, &tmp_list));
  EXPECT_TRUE(tmp_list.empty());
  tmp_list.clear();
  EXPECT_TRUE(storage_->Get(id2, &tmp_list));
  util::ExpectListsEqual(tmp_list, list2);
}

// Tests using Set to replace existing entries.
TEST_F(AppNotificationStorageTest, ReplaceExisting) {
  std::string id = extension_test_util::MakeId("1");
  AppNotificationList list1;
  AppNotificationList list2;
  util::AddNotifications(&list1, id, 5, "one");
  util::AddNotifications(&list2, id, 7, "two");

  // Put list1 in, then replace with list2 and verify we get list2 back.
  EXPECT_TRUE(storage_->Set(id, list1));
  EXPECT_TRUE(storage_->Set(id, list2));
  AppNotificationList tmp_list;
  EXPECT_TRUE(storage_->Get(id, &tmp_list));
  util::ExpectListsEqual(list2, tmp_list);
}

}  // namespace extensions

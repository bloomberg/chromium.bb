// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/app_notification_test_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "content/common/notification_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace util = app_notification_test_util;

class AppNotificationManagerTest : public testing::Test {
 public:
  AppNotificationManagerTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE) {}

  ~AppNotificationManagerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    file_thread_.Start();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new TestingProfile(temp_dir_.path()));
    InitializeManager();
  }

  virtual void TearDown() OVERRIDE {
    mgr_ = NULL;
    // Let the manager's shutdown of storage on the file thread complete.
    WaitForFileThread();
  }

 protected:
  void InitializeManager() {
    if (mgr_.get())
      WaitForFileThread();
    mgr_ = new AppNotificationManager(profile_.get());
    mgr_->Init();
    WaitForFileThread();
  }

  static void PostQuitToUIThread() {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            new MessageLoop::QuitTask());
  }

  static void WaitForFileThread() {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&PostQuitToUIThread));
    MessageLoop::current()->Run();
  }

  MessageLoop ui_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<AppNotificationManager> mgr_;
};

TEST_F(AppNotificationManagerTest, Simple) {
  std::string id = extension_test_util::MakeId("whatever");
  AppNotificationList list;
  util::AddNotifications(&list, 2, "foo");
  util::AddCopiesFromList(mgr_.get(), id, list);

  // Cause |mgr_| to be recreated, re-reading from its storage.
  InitializeManager();

  const AppNotification* tmp = mgr_->GetLast(id);
  EXPECT_TRUE(tmp && list[1]->Equals(*tmp));
  const AppNotificationList* tmp_list = mgr_->GetAll(id);
  ASSERT_TRUE(tmp_list != NULL);
  util::ExpectListsEqual(list, *tmp_list);
  mgr_->ClearAll(id);
  EXPECT_EQ(NULL, mgr_->GetLast(id));
  EXPECT_EQ(NULL, mgr_->GetAll(id));
}

// Test that AppNotificationManager correctly lists to EXTENSION_UNINSTALLED
// notifications and removes associated data when that happens.
TEST_F(AppNotificationManagerTest, ExtensionUninstall) {
  // Add some items from two test extension ids.
  std::string id1 = extension_test_util::MakeId("id1");
  std::string id2 = extension_test_util::MakeId("id2");
  AppNotificationList list1;
  AppNotificationList list2;
  util::AddNotifications(&list1, 5, "foo1");
  util::AddNotifications(&list2, 3, "foo2");
  util::AddCopiesFromList(mgr_.get(), id1, list1);
  util::AddCopiesFromList(mgr_.get(), id2, list2);
  util::ExpectListsEqual(list1, *mgr_->GetAll(id1));
  util::ExpectListsEqual(list2, *mgr_->GetAll(id2));

  // Send the uninstall notification for extension id1.
  DictionaryValue dict;
  dict.SetString(extension_manifest_keys::kName, "Test Extension");
  dict.SetString(extension_manifest_keys::kVersion, "0.1");
  std::string error;
  scoped_refptr<Extension> extension = Extension::CreateWithId(
      temp_dir_.path().AppendASCII("dummy"), Extension::INTERNAL,
      dict, 0, id1, &error);
  UninstalledExtensionInfo info(*extension.get());
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      Source<Profile>(profile_.get()),
      Details<UninstalledExtensionInfo>(&info));

  // The id1 items should be gone but the id2 items should still be there.
  EXPECT_EQ(NULL, mgr_->GetLast(id1));
  EXPECT_EQ(NULL, mgr_->GetAll(id1));
  util::ExpectListsEqual(list2, *mgr_->GetAll(id2));
}

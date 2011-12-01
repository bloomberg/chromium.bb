// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/app_notification_test_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

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
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<AppNotificationManager> mgr_;
};

TEST_F(AppNotificationManagerTest, Simple) {
  std::string id = extension_test_util::MakeId("whatever");
  AppNotificationList list;
  util::AddNotifications(&list, id, 2, "foo");
  EXPECT_TRUE(util::AddCopiesFromList(mgr_.get(), list));

  // Cause |mgr_| to be recreated, re-reading from its storage.
  InitializeManager();

  const AppNotification* tmp = mgr_->GetLast(id);
  ASSERT_TRUE(tmp);
  EXPECT_EQ(list[1]->guid(), tmp->guid());
  EXPECT_EQ(list[1]->extension_id(), tmp->extension_id());
  EXPECT_EQ(list[1]->is_local(), tmp->is_local());
  EXPECT_TRUE(list[1]->Equals(*tmp));
  const AppNotificationList* tmp_list = mgr_->GetAll(id);
  ASSERT_TRUE(tmp_list != NULL);
  util::ExpectListsEqual(list, *tmp_list);
  mgr_->ClearAll(id);
  EXPECT_EQ(NULL, mgr_->GetLast(id));
  EXPECT_EQ(NULL, mgr_->GetAll(id));
}

// Test that AppNotificationManager correctly listens to EXTENSION_UNINSTALLED
// notifications and removes associated data when that happens.
#ifdef ADDRESS_SANITIZER
// This test crashes under ASan, see http://crbug.com/100156
#define MAYBE_ExtensionUninstall DISABLED_ExtensionUninstall
#else
#define MAYBE_ExtensionUninstall ExtensionUninstall
#endif
TEST_F(AppNotificationManagerTest, MAYBE_ExtensionUninstall) {
  // Add some items from two test extension ids.
  std::string id1 = extension_test_util::MakeId("id1");
  std::string id2 = extension_test_util::MakeId("id2");
  AppNotificationList list1;
  AppNotificationList list2;
  util::AddNotifications(&list1, id1, 5, "foo1");
  util::AddNotifications(&list2, id2, 3, "foo2");
  util::AddCopiesFromList(mgr_.get(), list1);
  util::AddCopiesFromList(mgr_.get(), list2);
  util::ExpectListsEqual(list1, *mgr_->GetAll(id1));
  util::ExpectListsEqual(list2, *mgr_->GetAll(id2));

  // Send the uninstall notification for extension id1.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile_.get()),
      content::Details<const std::string>(&id1));

  // The id1 items should be gone but the id2 items should still be there.
  EXPECT_EQ(NULL, mgr_->GetLast(id1));
  EXPECT_EQ(NULL, mgr_->GetAll(id1));
  util::ExpectListsEqual(list2, *mgr_->GetAll(id2));
}

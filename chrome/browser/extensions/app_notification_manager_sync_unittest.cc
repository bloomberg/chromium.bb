// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/app_notification.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/sync/protocol/app_notification_specifics.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

// Extract notification guid from SyncData.
std::string GetGuid(const SyncData& sync_data) {
  return sync_data.GetSpecifics().GetExtension(
      sync_pb::app_notification).guid();
}

// Dummy SyncChangeProcessor used to help review what SyncChanges are pushed
// back up to Sync.
class TestChangeProcessor : public SyncChangeProcessor {
 public:
  TestChangeProcessor() { }
  virtual ~TestChangeProcessor() { }

  // Store a copy of all the changes passed in so we can examine them later.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) {
    // change_map_.erase(change_map_.begin(), change_map_.end());
    for (SyncChangeList::const_iterator iter = change_list.begin();
        iter != change_list.end(); ++iter) {
      change_map_[GetGuid(iter->sync_data())] = *iter;
    }

    return SyncError();
  }

  bool ContainsGuid(const std::string& guid) {
    return change_map_.find(guid) != change_map_.end();
  }

  SyncChange GetChangeByGuid(const std::string& guid) {
    DCHECK(ContainsGuid(guid));
    return change_map_[guid];
  }

  int change_list_size() { return change_map_.size(); }

 private:
  // Track the changes received in ProcessSyncChanges.
  std::map<std::string, SyncChange> change_map_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeProcessor);
};

}  // namespace

class AppNotificationManagerSyncTest : public testing::Test {
 public:
  AppNotificationManagerSyncTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE) { }

  ~AppNotificationManagerSyncTest() {
    model_ = NULL;
  }

  virtual void SetUp() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    file_thread_.Start();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new TestingProfile(temp_dir_.path()));
    model_ = new AppNotificationManager(profile_.get());
    model_->Init();

    WaitForFileThread();
    ASSERT_TRUE(model_->loaded());
  }

  virtual void TearDown() {
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

  AppNotificationManager* model() { return model_.get(); }
  TestChangeProcessor* processor() { return &processor_; }

  // Creates a notification whose properties are set from the given integer.
  static AppNotification* CreateNotification(int suffix) {
    return CreateNotification(false, suffix);
  }
  static AppNotification* CreateNotification(bool is_local, int suffix) {
    std::string s = base::IntToString(suffix);
    return CreateNotification(
        is_local, suffix, "guid" + s, "ext" + s, "text" + s, "body" + s,
        "http://www.url" + s + ".com", "link text " + s);
  }
  static AppNotification* CreateNotification(
      bool is_local, int suffix, const std::string& extension_id) {
    std::string s = base::IntToString(suffix);
    return CreateNotification(
        is_local, suffix, "guid" + s, extension_id, "text" + s, "body" + s,
        "http://www.url" + s + ".com", "link text " + s);
  }

  // Creates a notification whose properties are set from the given integer
  // but does not set link url and link text.
  static AppNotification* CreateNotificationNoLink(int suffix) {
    return CreateNotificationNoLink(false, suffix);
  }
  static AppNotification* CreateNotificationNoLink(bool is_local, int suffix) {
    std::string s = base::IntToString(suffix);
    return CreateNotification(
        is_local, suffix,
        "guid" + s, "ext" + s, "text" + s, "body" + s, "", "");
  }

  // link_url and link_text are only set if the passed in values are not empty.
  static AppNotification* CreateNotification(bool is_local,
                                             int64 time,
                                             const std::string& guid,
                                             const std::string& extension_id,
                                             const std::string& title,
                                             const std::string& body,
                                             const std::string& link_url,
                                             const std::string& link_text) {
    AppNotification* notif = new AppNotification(
        is_local, base::Time::FromInternalValue(time),
        guid, extension_id, title, body);
    if (!link_url.empty())
      notif->set_link_url(GURL(link_url));
    if (!link_text.empty())
      notif->set_link_text(link_text);
    return notif;
  }

  static SyncData CreateSyncData(int suffix) {
    scoped_ptr<AppNotification> notif(CreateNotification(suffix));
    return AppNotificationManager::CreateSyncDataFromNotification(*notif);
  }
  static SyncData CreateSyncData(int suffix, const std::string& extension_id) {
    scoped_ptr<AppNotification> notif(
        CreateNotification(false, suffix, extension_id));
    return AppNotificationManager::CreateSyncDataFromNotification(*notif);
  }

  // Helper to create SyncChange. Takes ownership of |notif|.
  static SyncChange CreateSyncChange(SyncChange::SyncChangeType type,
                                     AppNotification* notif) {
    // Take control of notif to clean it up after we create data out of it.
    scoped_ptr<AppNotification> scoped_notif(notif);
    return SyncChange(
        type, AppNotificationManager::CreateSyncDataFromNotification(*notif));
  }

  void AssertSyncChange(const SyncChange& change,
                        SyncChange::SyncChangeType type,
                        const AppNotification& notif) {
    ASSERT_EQ(type, change.change_type());
    scoped_ptr<AppNotification> notif2(
        AppNotificationManager::CreateNotificationFromSyncData(
            change.sync_data()));
    ASSERT_TRUE(notif.Equals(*notif2));
  }

 protected:
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  // We keep two TemplateURLServices to test syncing between them.
  ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<AppNotificationManager> model_;

  TestChangeProcessor processor_;

  DISALLOW_COPY_AND_ASSIGN(AppNotificationManagerSyncTest);
};

// Create an AppNotification, convert it to SyncData and convert it back.
TEST_F(AppNotificationManagerSyncTest, NotificationToSyncDataToNotification) {
  {  // Partial properties set.
    scoped_ptr<AppNotification> notif1(CreateNotificationNoLink(1));
    SyncData sync_data =
        AppNotificationManager::CreateSyncDataFromNotification(*notif1);
    scoped_ptr<AppNotification> notif2(
        AppNotificationManager::CreateNotificationFromSyncData(sync_data));
    EXPECT_TRUE(notif2.get());
    EXPECT_TRUE(notif1->Equals(*notif2));
  }
  {  // All properties set.
    scoped_ptr<AppNotification> notif1(CreateNotification(1));
    SyncData sync_data =
        AppNotificationManager::CreateSyncDataFromNotification(*notif1);
    scoped_ptr<AppNotification> notif2(
        AppNotificationManager::CreateNotificationFromSyncData(sync_data));
    EXPECT_TRUE(notif2.get());
    EXPECT_TRUE(notif1->Equals(*notif2));
  }
}

// GetAllSyncData returns all notifications since none are marked local only.
TEST_F(AppNotificationManagerSyncTest, GetAllSyncDataNoLocal) {
  model()->Add(CreateNotificationNoLink(1));
  model()->Add(CreateNotification(2));
  model()->Add(CreateNotification(3));
  SyncDataList all_sync_data =
      model()->GetAllSyncData(syncable::APP_NOTIFICATIONS);

  EXPECT_EQ(3U, all_sync_data.size());

  for (SyncDataList::const_iterator iter = all_sync_data.begin();
      iter != all_sync_data.end(); ++iter) {
    scoped_ptr<AppNotification> notif1(
        AppNotificationManager::CreateNotificationFromSyncData(*iter));

    const std::string& guid = notif1->guid();
    const std::string& ext_id = notif1->extension_id();

    const AppNotification* notif2 = model()->GetNotification(ext_id, guid);
    ASSERT_TRUE(notif1->Equals(*notif2));
  }
}

// GetAllSyncData should not return notifications marked as local only.
TEST_F(AppNotificationManagerSyncTest, GetAllSyncDataSomeLocal) {
  model()->Add(CreateNotificationNoLink(1));
  model()->Add(CreateNotification(true, 2));
  model()->Add(CreateNotification(3));
  model()->Add(CreateNotification(true, 4));
  model()->Add(CreateNotification(5));
  SyncDataList all_sync_data =
      model()->GetAllSyncData(syncable::APP_NOTIFICATIONS);

  EXPECT_EQ(3U, all_sync_data.size());

  for (SyncDataList::const_iterator iter = all_sync_data.begin();
      iter != all_sync_data.end(); ++iter) {
    scoped_ptr<AppNotification> notif1(
        AppNotificationManager::CreateNotificationFromSyncData(*iter));
    const std::string& guid = notif1->guid();
    const std::string& ext_id = notif1->extension_id();

    const AppNotification* notif2 = model()->GetNotification(ext_id, guid);
    ASSERT_TRUE(notif1->Equals(*notif2));
  }
}

// Model assocation: both models are empty.
TEST_F(AppNotificationManagerSyncTest, ModelAssocBothEmpty) {
  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      SyncDataList(),  // Empty.
      processor());

  EXPECT_EQ(0U, model()->GetAllSyncData(syncable::APP_NOTIFICATIONS).size());
  EXPECT_EQ(0, processor()->change_list_size());
}

// Model assocation: empty sync model and non-empty local model.
TEST_F(AppNotificationManagerSyncTest, ModelAssocModelEmpty) {
  SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(1));
  initial_data.push_back(CreateSyncData(2));
  initial_data.push_back(CreateSyncData(3));
  initial_data.push_back(CreateSyncData(4));

  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      initial_data,
      processor());

  EXPECT_EQ(4U, model()->GetAllSyncData(syncable::APP_NOTIFICATIONS).size());
  // Model should all of the initial sync data.
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    scoped_ptr<AppNotification> notif1(
        AppNotificationManager::CreateNotificationFromSyncData(*iter));
    const std::string& ext_id = notif1->extension_id();
    const std::string& guid = notif1->guid();
    const AppNotification* notif2 = model()->GetNotification(ext_id, guid);
    EXPECT_TRUE(notif2);
    EXPECT_TRUE(notif1->Equals(*notif2));
  }

  EXPECT_EQ(0, processor()->change_list_size());
}

// Model has some notifications, some of them are local only. Sync has some
// notifications. No items match up.
TEST_F(AppNotificationManagerSyncTest, ModelAssocBothNonEmptyNoOverlap) {
  AppNotification* n1 = CreateNotification(1);
  model()->Add(n1);
  AppNotification* n2 = CreateNotification(true, 2);
  model()->Add(n2);
  AppNotification* n3 = CreateNotification(3);
  model()->Add(n3);

  SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(4));
  initial_data.push_back(CreateSyncData(5));
  initial_data.push_back(CreateSyncData(6));
  initial_data.push_back(CreateSyncData(7));

  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      initial_data,
      processor());

  EXPECT_EQ(6U, model()->GetAllSyncData(syncable::APP_NOTIFICATIONS).size());
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    scoped_ptr<AppNotification> notif1(
        AppNotificationManager::CreateNotificationFromSyncData(*iter));
    const std::string& ext_id = notif1->extension_id();
    const std::string& guid = notif1->guid();
    const AppNotification* notif2 = model()->GetNotification(ext_id, guid);
    EXPECT_TRUE(notif2);
    EXPECT_TRUE(notif1->Equals(*notif2));
  }
  EXPECT_TRUE(model()->GetNotification(n1->extension_id(), n1->guid()));
  EXPECT_TRUE(model()->GetNotification(n2->extension_id(), n2->guid()));
  EXPECT_TRUE(model()->GetNotification(n3->extension_id(), n3->guid()));

  EXPECT_EQ(2, processor()->change_list_size());
  EXPECT_TRUE(processor()->ContainsGuid(n1->guid()));
  EXPECT_EQ(SyncChange::ACTION_ADD,
            processor()->GetChangeByGuid(n1->guid()).change_type());
  EXPECT_FALSE(processor()->ContainsGuid(n2->guid()));
  EXPECT_TRUE(processor()->ContainsGuid(n3->guid()));
  EXPECT_EQ(SyncChange::ACTION_ADD,
            processor()->GetChangeByGuid(n3->guid()).change_type());
}

// Model has some notifications, some of them are local only. Sync has some
// notifications. Some items match up.
TEST_F(AppNotificationManagerSyncTest, ModelAssocBothNonEmptySomeOverlap) {
  AppNotification* n1 = CreateNotification(1);
  model()->Add(n1);
  AppNotification* n2 = CreateNotification(true, 2);
  model()->Add(n2);
  AppNotification* n3 = CreateNotification(3);
  model()->Add(n3);
  AppNotification* n4 = CreateNotification(4);
  model()->Add(n4);

  SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(5));
  initial_data.push_back(
      AppNotificationManager::CreateSyncDataFromNotification(*n1));
  initial_data.push_back(CreateSyncData(6));
  initial_data.push_back(
      AppNotificationManager::CreateSyncDataFromNotification(*n4));
  initial_data.push_back(CreateSyncData(7));

  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      initial_data,
      processor());

  EXPECT_EQ(6U, model()->GetAllSyncData(syncable::APP_NOTIFICATIONS).size());
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    scoped_ptr<AppNotification> notif1(
        AppNotificationManager::CreateNotificationFromSyncData(*iter));
    const std::string& ext_id = notif1->extension_id();
    const std::string& guid = notif1->guid();
    const AppNotification* notif2 = model()->GetNotification(ext_id, guid);
    EXPECT_TRUE(notif2);
    EXPECT_TRUE(notif1->Equals(*notif2));
  }
  EXPECT_TRUE(model()->GetNotification(n1->extension_id(), n1->guid()));
  EXPECT_TRUE(model()->GetNotification(n2->extension_id(), n2->guid()));
  EXPECT_TRUE(model()->GetNotification(n3->extension_id(), n3->guid()));
  EXPECT_TRUE(model()->GetNotification(n4->extension_id(), n4->guid()));

  EXPECT_EQ(1, processor()->change_list_size());
  EXPECT_FALSE(processor()->ContainsGuid(n1->guid()));
  EXPECT_FALSE(processor()->ContainsGuid(n2->guid()));
  EXPECT_TRUE(processor()->ContainsGuid(n3->guid()));
  EXPECT_EQ(SyncChange::ACTION_ADD,
            processor()->GetChangeByGuid(n3->guid()).change_type());
  EXPECT_FALSE(processor()->ContainsGuid(n4->guid()));
}

// When an item that matches up in model and sync is different, an error
// should be returned.
TEST_F(AppNotificationManagerSyncTest, ModelAssocBothNonEmptyTitleMismatch) {
  AppNotification* n1 = CreateNotification(1);
  model()->Add(n1);
  AppNotification* n2 = CreateNotification(true, 2);
  model()->Add(n2);

  SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(1));
  scoped_ptr<AppNotification> n1_a(CreateNotification(
      n1->is_local(), n1->creation_time().ToInternalValue(),
      n1->guid(), n1->extension_id(),
      n1->title() + "_changed", // different title
      n1->body(), n1->link_url().spec(), n1->link_text()));
  initial_data.push_back(
      AppNotificationManager::CreateSyncDataFromNotification(*n1_a));

  SyncError sync_error = model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      initial_data,
      processor());

  EXPECT_TRUE(sync_error.IsSet());
  EXPECT_EQ(syncable::APP_NOTIFICATIONS, sync_error.type());
  EXPECT_EQ(0, processor()->change_list_size());
}

// When an item in sync matches with a local-only item in model, an error
// should be returned.
TEST_F(AppNotificationManagerSyncTest, ModelAssocBothNonEmptyMatchesLocal) {
  AppNotification* n1 = CreateNotification(1);
  model()->Add(n1);
  AppNotification* n2 = CreateNotification(true, 2);
  model()->Add(n2);

  SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(1));
  scoped_ptr<AppNotification> n2_a(CreateNotification(2));
  initial_data.push_back(
      AppNotificationManager::CreateSyncDataFromNotification(*n2_a));

  SyncError sync_error = model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      initial_data,
      processor());

  EXPECT_TRUE(sync_error.IsSet());
  EXPECT_EQ(syncable::APP_NOTIFICATIONS, sync_error.type());
  EXPECT_EQ(0, processor()->change_list_size());
}

// Process sync changes when model is empty.
TEST_F(AppNotificationManagerSyncTest, ProcessSyncChangesEmptyModel) {
  // We initially have no data.
  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      SyncDataList(),
      processor());

  // Set up a bunch of ADDs.
  SyncChangeList changes;
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(1)));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(2)));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(3)));

  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(3U, model()->GetAllSyncData(syncable::APP_NOTIFICATIONS).size());
  EXPECT_EQ(0, processor()->change_list_size());
}

// Process sync changes when model is not empty.
TEST_F(AppNotificationManagerSyncTest, ProcessSyncChangesNonEmptyModel) {
  AppNotification* n1 = CreateNotification(1);
  model()->Add(n1);
  AppNotification* n2 = CreateNotification(2);
  model()->Add(n2);
  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      SyncDataList(),
      processor());

  // Some adds and some deletes.
  SyncChangeList changes;
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(3)));
  changes.push_back(CreateSyncChange(SyncChange::ACTION_DELETE, n1->Copy()));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(4)));

  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(3U, model()->GetAllSyncData(syncable::APP_NOTIFICATIONS).size());
  EXPECT_EQ(2, processor()->change_list_size());
}

// Process over 15 changes changes when model is not empty.
TEST_F(AppNotificationManagerSyncTest, ProcessSyncChangesEmptyModelWithMax) {
  const std::string& ext_id = "e1";
  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      SyncDataList(),
      processor());
  for (unsigned int i = 0;
       i < AppNotificationManager::kMaxNotificationPerApp * 2; i++) {
    SyncChangeList changes;
    changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(false, i, ext_id)));
    model()->ProcessSyncChanges(FROM_HERE, changes);
    if (i < AppNotificationManager::kMaxNotificationPerApp) {
      EXPECT_EQ(i + 1,
                model()->GetAllSyncData(syncable::APP_NOTIFICATIONS).size());
    } else {
      EXPECT_EQ(AppNotificationManager::kMaxNotificationPerApp,
        model()->GetAllSyncData(syncable::APP_NOTIFICATIONS).size());
      for (unsigned int j = i; j > i - 5; j--) {
        EXPECT_EQ(
            AppNotificationManager::kMaxNotificationPerApp,
            model()->GetAllSyncData(syncable::APP_NOTIFICATIONS).size());
      }
    }
  }
}

// Process sync changes should return error if model association is not done.
TEST_F(AppNotificationManagerSyncTest,
       ProcessSyncChangesErrorModelAssocNotDone) {
  SyncChangeList changes;

  SyncError sync_error = model()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(sync_error.IsSet());
  EXPECT_EQ(syncable::APP_NOTIFICATIONS, sync_error.type());
  EXPECT_EQ(0, processor()->change_list_size());
}

// Stop syncing sets state correctly.
TEST_F(AppNotificationManagerSyncTest, StopSyncing) {
  EXPECT_FALSE(model()->sync_processor_);
  EXPECT_FALSE(model()->models_associated_);

  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      SyncDataList(),
      processor());

  EXPECT_TRUE(model()->sync_processor_);
  EXPECT_TRUE(model()->models_associated_);

  model()->StopSyncing(syncable::APP_NOTIFICATIONS);
  EXPECT_FALSE(model()->sync_processor_);
  EXPECT_FALSE(model()->models_associated_);
}

// Adds get pushed to sync but local only are skipped.
TEST_F(AppNotificationManagerSyncTest, AddsGetsSynced) {
  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      SyncDataList(),
      processor());

  AppNotification* n1 = CreateNotification(1);
  model()->Add(n1);
  AppNotification* n2 = CreateNotification(2);
  model()->Add(n2);
  AppNotification* n3 = CreateNotification(true, 2);
  model()->Add(n3);

  EXPECT_EQ(2, processor()->change_list_size());
  EXPECT_TRUE(processor()->ContainsGuid(n1->guid()));
  SyncChange c1 = processor()->GetChangeByGuid(n1->guid());
  AssertSyncChange(c1, SyncChange::ACTION_ADD, *n1);
  SyncChange c2 = processor()->GetChangeByGuid(n2->guid());
  AssertSyncChange(c2, SyncChange::ACTION_ADD, *n2);
}

// Clear all gets pushed to sync.
TEST_F(AppNotificationManagerSyncTest, ClearAllGetsSynced) {
  const std::string& ext_id = "e1";
  scoped_ptr<AppNotification> n1(CreateNotification(false, 1, ext_id));
  scoped_ptr<AppNotification> n2(CreateNotification(false, 2, ext_id));
  scoped_ptr<AppNotification> n3(CreateNotification(false, 3, ext_id));
  scoped_ptr<AppNotification> n4(CreateNotification(4));

  SyncDataList initial_data;
  initial_data.push_back(
      AppNotificationManager::CreateSyncDataFromNotification(*n1));
  initial_data.push_back(
      AppNotificationManager::CreateSyncDataFromNotification(*n2));
  initial_data.push_back(
      AppNotificationManager::CreateSyncDataFromNotification(*n3));
  initial_data.push_back(
      AppNotificationManager::CreateSyncDataFromNotification(*n4));
  model()->MergeDataAndStartSyncing(
      syncable::APP_NOTIFICATIONS,
      initial_data,
      processor());

  model()->ClearAll(ext_id);

  EXPECT_EQ(3, processor()->change_list_size());
  EXPECT_TRUE(processor()->ContainsGuid(n1->guid()));
  SyncChange c1 = processor()->GetChangeByGuid(n1->guid());
  AssertSyncChange(c1, SyncChange::ACTION_DELETE, *n1);
  SyncChange c2 = processor()->GetChangeByGuid(n2->guid());
  AssertSyncChange(c2, SyncChange::ACTION_DELETE, *n2);
  SyncChange c3 = processor()->GetChangeByGuid(n3->guid());
  AssertSyncChange(c3, SyncChange::ACTION_DELETE, *n3);
}

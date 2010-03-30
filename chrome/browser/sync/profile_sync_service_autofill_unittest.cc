// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/waitable_event.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_change_processor.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "chrome/test/profile_mock.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::Time;
using base::WaitableEvent;
using browser_sync::AutofillChangeProcessor;
using browser_sync::AutofillDataTypeController;
using browser_sync::AutofillModelAssociator;
using browser_sync::SyncBackendHostMock;
using browser_sync::TestIdFactory;
using browser_sync::UnrecoverableErrorHandler;
using sync_api::SyncManager;
using sync_api::UserShare;
using syncable::BASE_VERSION;
using syncable::CREATE;
using syncable::DirectoryManager;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::MutableEntry;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_VERSION;
using syncable::SPECIFICS;
using syncable::ScopedDirLookup;
using syncable::UNIQUE_SERVER_TAG;
using syncable::UNITTEST;
using syncable::WriteTransaction;
using testing::_;
using testing::DoAll;
using testing::DoDefault;
using testing::ElementsAre;
using testing::Invoke;
using testing::Return;
using testing::SetArgumentPointee;

class WebDatabaseMock : public WebDatabase {
 public:
  MOCK_METHOD2(RemoveFormElement,
               bool(const string16& name, const string16& value));  // NOLINT
  MOCK_METHOD1(GetAllAutofillEntries,
               bool(std::vector<AutofillEntry>* entries));  // NOLINT
  MOCK_METHOD3(GetAutofillTimestamps,
               bool(const string16& name,  // NOLINT
                    const string16& value,
                    std::vector<base::Time>* timestamps));
  MOCK_METHOD1(UpdateAutofillEntries,
               bool(const std::vector<AutofillEntry>& entries));  // NOLINT
};

class WebDataServiceFake : public WebDataService {
 public:
  virtual bool IsDatabaseLoaded() {
    return true;
  }

  // Note that we inject the WebDatabase through the
  // ProfileSyncFactory mock.
  virtual WebDatabase* GetDatabase() {
    return NULL;
  }
};

ACTION_P3(MakeAutofillSyncComponents, service, wd, dtc) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  AutofillModelAssociator* model_associator =
      new AutofillModelAssociator(service, wd, dtc);
  AutofillChangeProcessor* change_processor =
      new AutofillChangeProcessor(model_associator, wd, dtc);
  return ProfileSyncFactory::SyncComponents(model_associator,
                                            change_processor);
}

class ProfileSyncServiceAutofillTest : public testing::Test {
 protected:
  ProfileSyncServiceAutofillTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        db_thread_(ChromeThread::DB) {
  }

  virtual void SetUp() {
    web_data_service_ = new WebDataServiceFake();
    db_thread_.Start();

    notification_service_ = new ThreadNotificationService(&db_thread_);
    notification_service_->Init();
  }

  virtual void TearDown() {
    service_.reset();
    notification_service_->TearDown();
    db_thread_.Stop();
    MessageLoop::current()->RunAllPending();
  }

  void StartSyncService(Task* task) {
    if (!service_.get()) {
      service_.reset(
          new TestingProfileSyncService(&factory_, &profile_, false));
      service_->AddObserver(&observer_);
      AutofillDataTypeController* data_type_controller =
          new AutofillDataTypeController(&factory_,
                                         &profile_,
                                         service_.get());

      EXPECT_CALL(factory_, CreateAutofillSyncComponents(_, _, _)).
          WillOnce(MakeAutofillSyncComponents(service_.get(),
                                              &web_database_,
                                              data_type_controller));
      EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
          WillOnce(MakeDataTypeManager(&backend_));

      EXPECT_CALL(profile_, GetWebDataService(_)).
          WillOnce(Return(web_data_service_.get()));

      // State changes once for the backend init and once for startup done.
      EXPECT_CALL(observer_, OnStateChanged()).
          WillOnce(InvokeTask(task)).
          WillOnce(QuitUIMessageLoop());
      service_->RegisterDataTypeController(data_type_controller);
      service_->Initialize();
      MessageLoop::current()->Run();
    }
  }

  void CreateAutofillRoot() {
    UserShare* user_share = service_->backend()->GetUserShareHandle();
    DirectoryManager* dir_manager = user_share->dir_manager.get();

    ScopedDirLookup dir(dir_manager, user_share->authenticated_name);
    ASSERT_TRUE(dir.good());

    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry node(&wtrans,
                      CREATE,
                      wtrans.root_id(),
                      browser_sync::kAutofillTag);
    node.Put(UNIQUE_SERVER_TAG, browser_sync::kAutofillTag);
    node.Put(IS_DIR, true);
    node.Put(SERVER_IS_DIR, false);
    node.Put(IS_UNSYNCED, false);
    node.Put(IS_UNAPPLIED_UPDATE, false);
    node.Put(SERVER_VERSION, 20);
    node.Put(BASE_VERSION, 20);
    node.Put(IS_DEL, false);
    node.Put(ID, ids_.MakeServer(browser_sync::kAutofillTag));
    sync_pb::EntitySpecifics specifics;
    specifics.MutableExtension(sync_pb::autofill);
    node.Put(SPECIFICS, specifics);
  }

  void AddAutofillSyncNode(const AutofillEntry& entry) {
    sync_api::WriteTransaction trans(
        service_->backend()->GetUserShareHandle());
    sync_api::ReadNode autofill_root(&trans);
    ASSERT_TRUE(autofill_root.InitByTagLookup(browser_sync::kAutofillTag));

    sync_api::WriteNode node(&trans);
    std::string tag = AutofillModelAssociator::KeyToTag(entry.key().name(),
                                                        entry.key().value());
    ASSERT_TRUE(node.InitUniqueByCreation(syncable::AUTOFILL,
                                          autofill_root,
                                          tag));
    AutofillChangeProcessor::WriteAutofill(&node, entry);
  }

  void GetAutofillEntriesFromSyncDB(std::vector<AutofillEntry>* entries) {
    sync_api::ReadTransaction trans(service_->backend()->GetUserShareHandle());
    sync_api::ReadNode autofill_root(&trans);
    ASSERT_TRUE(autofill_root.InitByTagLookup(browser_sync::kAutofillTag));

    int64 child_id = autofill_root.GetFirstChildId();
    while (child_id != sync_api::kInvalidId) {
      sync_api::ReadNode child_node(&trans);
      ASSERT_TRUE(child_node.InitByIdLookup(child_id));

      const sync_pb::AutofillSpecifics& autofill(
          child_node.GetAutofillSpecifics());
      AutofillKey key(UTF8ToUTF16(autofill.name()),
                      UTF8ToUTF16(autofill.value()));
      std::vector<base::Time> timestamps;
      int timestamps_count = autofill.usage_timestamp_size();
      for (int i = 0; i < timestamps_count; ++i) {
        timestamps.push_back(Time::FromInternalValue(
            autofill.usage_timestamp(i)));
      }
      entries->push_back(AutofillEntry(key, timestamps));
      child_id = child_node.GetSuccessorId();
    }
  }

  void SetIdleChangeProcessorExpectations() {
    EXPECT_CALL(web_database_, RemoveFormElement(_, _)).Times(0);
    EXPECT_CALL(web_database_, GetAutofillTimestamps(_, _, _)).Times(0);
    EXPECT_CALL(web_database_, UpdateAutofillEntries(_)).Times(0);
  }

  static AutofillEntry MakeAutofillEntry(const char* name,
                                         const char* value,
                                         time_t timestamp0,
                                         time_t timestamp1) {
    std::vector<Time> timestamps;
    if (timestamp0 > 0)
      timestamps.push_back(Time::FromTimeT(timestamp0));
    if (timestamp1 > 0)
      timestamps.push_back(Time::FromTimeT(timestamp1));
    return AutofillEntry(
        AutofillKey(ASCIIToUTF16(name), ASCIIToUTF16(value)), timestamps);
  }

  static AutofillEntry MakeAutofillEntry(const char* name,
                                         const char* value,
                                         time_t timestamp) {
    return MakeAutofillEntry(name, value, timestamp, -1);
  }

  friend class CreateAutofillRootTask;
  friend class AddAutofillEntriesTask;

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  ChromeThread db_thread_;
  scoped_refptr<ThreadNotificationService> notification_service_;

  scoped_ptr<TestingProfileSyncService> service_;
  ProfileMock profile_;
  ProfileSyncFactoryMock factory_;
  ProfileSyncServiceObserverMock observer_;
  SyncBackendHostMock backend_;
  WebDatabaseMock web_database_;
  scoped_refptr<WebDataService> web_data_service_;

  TestIdFactory ids_;
};

class CreateAutofillRootTask : public Task {
 public:
  explicit CreateAutofillRootTask(ProfileSyncServiceAutofillTest* test)
      : test_(test) {
  }

  virtual void Run() {
    test_->CreateAutofillRoot();
  }

 private:
  ProfileSyncServiceAutofillTest* test_;
};

class AddAutofillEntriesTask : public Task {
 public:
  AddAutofillEntriesTask(ProfileSyncServiceAutofillTest* test,
                         const std::vector<AutofillEntry>& entries)
      : test_(test), entries_(entries) {
  }

  virtual void Run() {
    test_->CreateAutofillRoot();
    for (size_t i = 0; i < entries_.size(); ++i) {
      test_->AddAutofillSyncNode(entries_[i]);
    }
  }

 private:
  ProfileSyncServiceAutofillTest* test_;
  const std::vector<AutofillEntry>& entries_;
};

// TODO(skrul): Test abort startup.
// TODO(skrul): Test processing of cloud changes.
TEST_F(ProfileSyncServiceAutofillTest, FailModelAssociation) {
  // Backend will be paused but not resumed.
  EXPECT_CALL(backend_, RequestPause()).
      WillOnce(testing::DoAll(Notify(NotificationType::SYNC_PAUSED),
                              testing::Return(true)));
  // Don't create the root autofill node so startup fails.
  StartSyncService(NULL);
  EXPECT_TRUE(service_->unrecoverable_error_detected());
}

TEST_F(ProfileSyncServiceAutofillTest, EmptyNativeEmptySync) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateAutofillRootTask task(this);
  StartSyncService(&task);
  std::vector<AutofillEntry> sync_entries;
  GetAutofillEntriesFromSyncDB(&sync_entries);
  EXPECT_EQ(0U, sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeEmptySync) {
  std::vector<AutofillEntry> entries;
  entries.push_back(MakeAutofillEntry("foo", "bar", 1));
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateAutofillRootTask task(this);
  StartSyncService(&task);
  std::vector<AutofillEntry> sync_entries;
  GetAutofillEntriesFromSyncDB(&sync_entries);
  ASSERT_EQ(1U, entries.size());
  EXPECT_TRUE(entries[0] == sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeWithDuplicatesEmptySync) {
  // There is buggy autofill code that allows duplicate name/value
  // pairs to exist in the database with separate pair_ids.
  std::vector<AutofillEntry> entries;
  entries.push_back(MakeAutofillEntry("foo", "bar", 1));
  entries.push_back(MakeAutofillEntry("dup", "", 2));
  entries.push_back(MakeAutofillEntry("dup", "", 3));
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateAutofillRootTask task(this);
  StartSyncService(&task);
  std::vector<AutofillEntry> sync_entries;
  GetAutofillEntriesFromSyncDB(&sync_entries);
  EXPECT_EQ(2U, sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncNoMerge) {
  AutofillEntry native_entry(MakeAutofillEntry("native", "entry", 1));
  AutofillEntry sync_entry(MakeAutofillEntry("sync", "entry", 2));

  std::vector<AutofillEntry> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));

  std::vector<AutofillEntry> sync_entries;
  sync_entries.push_back(sync_entry);
  AddAutofillEntriesTask task(this, sync_entries);

  EXPECT_CALL(web_database_, UpdateAutofillEntries(ElementsAre(sync_entry))).
      WillOnce(Return(true));
  StartSyncService(&task);

  std::set<AutofillEntry> expected;
  expected.insert(native_entry);
  expected.insert(sync_entry);

  std::vector<AutofillEntry> new_sync_entries;
  GetAutofillEntriesFromSyncDB(&new_sync_entries);
  std::set<AutofillEntry> new_sync_entries_set(new_sync_entries.begin(),
                                               new_sync_entries.end());
  EXPECT_TRUE(expected == new_sync_entries_set);
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMerge) {
  AutofillEntry native_entry(MakeAutofillEntry("merge", "entry", 1));
  AutofillEntry sync_entry(MakeAutofillEntry("merge", "entry", 2));
  AutofillEntry merged_entry(MakeAutofillEntry("merge", "entry", 1, 2));

  std::vector<AutofillEntry> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));

  std::vector<AutofillEntry> sync_entries;
  sync_entries.push_back(sync_entry);
  AddAutofillEntriesTask task(this, sync_entries);

  EXPECT_CALL(web_database_, UpdateAutofillEntries(ElementsAre(merged_entry))).
      WillOnce(Return(true));
  StartSyncService(&task);

  std::vector<AutofillEntry> new_sync_entries;
  GetAutofillEntriesFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(merged_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAdd) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateAutofillRootTask task(this);
  StartSyncService(&task);

  AutofillEntry added_entry(MakeAutofillEntry("added", "entry", 1));
  std::vector<base::Time> timestamps(added_entry.timestamps());

  EXPECT_CALL(web_database_, GetAutofillTimestamps(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(timestamps), Return(true)));

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::ADD, added_entry.key()));
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Details<AutofillChangeList>(&changes));

  std::vector<AutofillEntry> new_sync_entries;
  GetAutofillEntriesFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(added_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeUpdate) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  CreateAutofillRootTask task(this);
  StartSyncService(&task);

  AutofillEntry updated_entry(MakeAutofillEntry("my", "entry", 1, 2));
  std::vector<base::Time> timestamps(updated_entry.timestamps());

  EXPECT_CALL(web_database_, GetAutofillTimestamps(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(timestamps), Return(true)));

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::UPDATE,
                                   updated_entry.key()));
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Details<AutofillChangeList>(&changes));

  std::vector<AutofillEntry> new_sync_entries;
  GetAutofillEntriesFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(updated_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemove) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  CreateAutofillRootTask task(this);
  StartSyncService(&task);

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                   original_entry.key()));
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Details<AutofillChangeList>(&changes));

  std::vector<AutofillEntry> new_sync_entries;
  GetAutofillEntriesFromSyncDB(&new_sync_entries);
  ASSERT_EQ(0U, new_sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeError) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  CreateAutofillRootTask task(this);
  StartSyncService(&task);

  // Inject an evil entry into the sync db to conflict with the same
  // entry added by the user.
  AutofillEntry evil_entry(MakeAutofillEntry("evil", "entry", 1));
  AddAutofillSyncNode(evil_entry);

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::ADD,
                                   evil_entry.key()));
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Details<AutofillChangeList>(&changes));

  // Wait for the PPS to shut everything down and signal us.
  EXPECT_CALL(observer_, OnStateChanged()).WillOnce(QuitUIMessageLoop());
  MessageLoop::current()->Run();
  EXPECT_TRUE(service_->unrecoverable_error_detected());

  // Ensure future autofill notifications don't crash.
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Details<AutofillChangeList>(&changes));
}

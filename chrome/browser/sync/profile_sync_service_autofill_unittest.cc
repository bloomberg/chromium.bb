// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_change_processor.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::Time;
using base::WaitableEvent;
using browser_sync::AutofillChangeProcessor;
using browser_sync::AutofillDataTypeController;
using browser_sync::AutofillModelAssociator;
using browser_sync::SyncBackendHost;
using browser_sync::UnrecoverableErrorHandler;
using sync_api::SyncManager;
using testing::_;
using testing::DoAll;
using testing::DoDefault;
using testing::Return;
using testing::SetArgumentPointee;

class TestAutofillModelAssociator
    : public TestModelAssociator<AutofillModelAssociator> {
 public:
  TestAutofillModelAssociator(ProfileSyncService* service,
                              WebDatabase* web_database,
                              UnrecoverableErrorHandler* error_handler)
      : TestModelAssociator<AutofillModelAssociator>(
          service, web_database, error_handler) {
  }
};

class WebDatabaseMock : public WebDatabase {
 public:
  MOCK_METHOD2(RemoveFormElement,
               bool(const string16& name, const string16& value));
  MOCK_METHOD1(GetAllAutofillEntries,
               bool(std::vector<AutofillEntry>* entries));
  MOCK_METHOD3(GetAutofillTimestamps,
               bool(const string16& name,
                    const string16& value,
                    std::vector<base::Time>* timestamps));
  MOCK_METHOD1(UpdateAutofillEntries,
               bool(const std::vector<AutofillEntry>& entries));
};

class ProfileMock : public TestingProfile {
 public:
  MOCK_METHOD1(GetWebDataService, WebDataService*(ServiceAccessType access));
};

class DBThreadNotificationService :
    public base::RefCountedThreadSafe<DBThreadNotificationService> {
 public:
  DBThreadNotificationService() : done_event_(false, false) {}

  void Init() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    ChromeThread::PostTask(
        ChromeThread::DB,
        FROM_HERE,
        NewRunnableMethod(this, &DBThreadNotificationService::InitTask));
    done_event_.Wait();
  }

  void TearDown() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    ChromeThread::PostTask(
        ChromeThread::DB,
        FROM_HERE,
        NewRunnableMethod(this, &DBThreadNotificationService::TearDownTask));
    done_event_.Wait();
  }

 private:
  friend class base::RefCountedThreadSafe<DBThreadNotificationService>;

  void InitTask() {
    service_.reset(new NotificationService());
    done_event_.Signal();
  }

  void TearDownTask() {
    service_.reset(NULL);
    done_event_.Signal();
  }

  WaitableEvent done_event_;
  scoped_ptr<NotificationService> service_;
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

ACTION(QuitUIMessageLoop) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  MessageLoop::current()->Quit();
}

ACTION_P3(MakeAutofillSyncComponents, service, wd, dtc) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  AutofillModelAssociator* model_associator =
      new TestAutofillModelAssociator(service, wd, dtc);
  AutofillChangeProcessor* change_processor =
      new AutofillChangeProcessor(model_associator, wd, service);
  return ProfileSyncFactory::SyncComponents(model_associator, change_processor);
}

class ProfileSyncServiceAutofillTest : public testing::Test {
 protected:
  ProfileSyncServiceAutofillTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        db_thread_(ChromeThread::DB),
        done_event_(false, false) {
  }

  virtual void SetUp() {
    web_data_service_ = new WebDataServiceFake();
    db_thread_.Start();

    notification_service_ = new DBThreadNotificationService();
    notification_service_->Init();
  }

  virtual void TearDown() {
    service_.reset();
    notification_service_->TearDown();
    db_thread_.Stop();
    MessageLoop::current()->RunAllPending();
  }

  void StartSyncService() {
    if (!service_.get()) {
      service_.reset(new TestProfileSyncService(&factory_, &profile_, false));
      service_->AddObserver(&observer_);
      AutofillDataTypeController* data_type_controller =
          new AutofillDataTypeController(&factory_,
                                         &profile_,
                                         service_.get());

      EXPECT_CALL(factory_, CreateAutofillSyncComponents(_, _, _)).
          WillOnce(MakeAutofillSyncComponents(service_.get(),
                                              &web_database_,
                                              data_type_controller));
      EXPECT_CALL(factory_, CreateDataTypeManager(_)).
          WillOnce(MakeDataTypeManager());

      EXPECT_CALL(profile_, GetWebDataService(_)).
          WillOnce(Return(web_data_service_.get()));

      // State changes once for the backend init and once for startup done.
      EXPECT_CALL(observer_, OnStateChanged()).
          WillOnce(DoDefault()).
          WillOnce(QuitUIMessageLoop());
      service_->RegisterDataTypeController(data_type_controller);
      service_->Initialize();
      MessageLoop::current()->Run();
    }
  }

  void GetAutofillEntriesFromSyncDB(std::vector<AutofillEntry>* entries) {
    sync_api::ReadTransaction trans(service_->backend()->GetUserShareHandle());
    int64 autofill_root_id =
        GetFakeServerTaggedNode(&trans, "google_chrome_autofill");
    if (autofill_root_id == sync_api::kInvalidId)
      return;

    sync_api::ReadNode autofill_root(&trans);
    if (!autofill_root.InitByIdLookup(autofill_root_id))
      return;

    int64 child_id = autofill_root.GetFirstChildId();
    while (child_id != sync_api::kInvalidId) {
      sync_api::ReadNode child_node(&trans);
      if (!child_node.InitByIdLookup(child_id))
        return;

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

  int64 GetFakeServerTaggedNode(sync_api::ReadTransaction* trans,
                                const std::string& tag) {
    std::wstring tag_wide;
    if (!UTF8ToWide(tag.c_str(), tag.length(), &tag_wide))
      return sync_api::kInvalidId;

    sync_api::ReadNode root(trans);
    root.InitByRootLookup();

    int64 last_child_id = sync_api::kInvalidId;
    for (int64 id = root.GetFirstChildId(); id != sync_api::kInvalidId; /***/) {
      sync_api::ReadNode child(trans);
      child.InitByIdLookup(id);
      last_child_id = id;
      if (tag_wide == child.GetTitle()) {
        return id;
      }
      id = child.GetSuccessorId();
    }

    return sync_api::kInvalidId;
  }

  void SetIdleChangeProcessorExpectations() {
    EXPECT_CALL(web_database_, RemoveFormElement(_, _)).Times(0);
    EXPECT_CALL(web_database_, GetAutofillTimestamps(_, _, _)).Times(0);
    EXPECT_CALL(web_database_, UpdateAutofillEntries(_)).Times(0);
  }

  static AutofillEntry MakeAutofillEntry(const char* name,
                                         const char* value,
                                         time_t timestamp) {
    std::vector<Time> timestamps;
    timestamps.push_back(Time::FromTimeT(timestamp));
    return AutofillEntry(
        AutofillKey(ASCIIToUTF16(name), ASCIIToUTF16(value)), timestamps);
  }


  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  ChromeThread db_thread_;
  WaitableEvent done_event_;
  scoped_refptr<DBThreadNotificationService> notification_service_;

  scoped_ptr<TestProfileSyncService> service_;
  ProfileMock profile_;
  ProfileSyncFactoryMock factory_;
  ProfileSyncServiceObserverMock observer_;
  WebDatabaseMock web_database_;
  scoped_refptr<WebDataService> web_data_service_;
};

// TODO(sync): Test unrecoverable error during MA.

TEST_F(ProfileSyncServiceAutofillTest, EmptyNativeEmptySync) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  StartSyncService();
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
  StartSyncService();
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
  StartSyncService();
  std::vector<AutofillEntry> sync_entries;
  GetAutofillEntriesFromSyncDB(&sync_entries);
  EXPECT_EQ(2U, sync_entries.size());
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/task.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/engine/model_changing_syncer_command.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_change_processor.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/glue/autofill_profile_change_processor.h"
#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"
#include "chrome/browser/sync/glue/autofill_profile_model_associator.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/syncable/autofill_migration.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/test/profile_mock.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::Time;
using base::WaitableEvent;
using browser_sync::AutofillChangeProcessor;
using browser_sync::AutofillDataTypeController;
using browser_sync::AutofillModelAssociator;
using browser_sync::AutofillProfileChangeProcessor;
using browser_sync::AutofillProfileDataTypeController;
using browser_sync::AutofillProfileModelAssociator;
using browser_sync::DataTypeController;
using browser_sync::GROUP_DB;
using browser_sync::kAutofillTag;
using browser_sync::SyncBackendHostForProfileSyncTest;
using browser_sync::UnrecoverableErrorHandler;
using syncable::CREATE_NEW_UPDATE_ITEM;
using syncable::AUTOFILL;
using syncable::BASE_VERSION;
using syncable::CREATE;
using syncable::DirectoryChangeEvent;
using syncable::GET_BY_SERVER_TAG;
using syncable::INVALID;
using syncable::MutableEntry;
using syncable::OriginalEntries;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_SPECIFICS;
using syncable::SPECIFICS;
using syncable::UNITTEST;
using syncable::WriterTag;
using syncable::WriteTransaction;
using testing::_;
using testing::DoAll;
using testing::DoDefault;
using testing::ElementsAre;
using testing::Eq;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgumentPointee;

namespace syncable {
class Id;
}

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
               bool(const std::vector<AutofillEntry>&));  // NOLINT
  MOCK_METHOD1(GetAutoFillProfiles,
               bool(std::vector<AutoFillProfile*>*));  // NOLINT
  MOCK_METHOD1(UpdateAutoFillProfile,
               bool(const AutoFillProfile&));  // NOLINT
  MOCK_METHOD1(AddAutoFillProfile,
               bool(const AutoFillProfile&));  // NOLINT
  MOCK_METHOD1(RemoveAutoFillProfile,
               bool(const std::string&));  // NOLINT
};

class ProfileSyncServiceAutofillTest;

template<class AutoFillProfile>
syncable::ModelType GetModelType() {
  return syncable::UNSPECIFIED;
}

template<>
syncable::ModelType GetModelType<AutofillEntry>() {
  return syncable::AUTOFILL;
}

template<>
syncable::ModelType GetModelType<AutoFillProfile>() {
  return syncable::AUTOFILL_PROFILE;
}

class WebDataServiceFake : public WebDataService {
 public:
  explicit WebDataServiceFake(WebDatabase* web_database)
      : web_database_(web_database) {}
  virtual bool IsDatabaseLoaded() {
    return true;
  }

  virtual WebDatabase* GetDatabase() {
    return web_database_;
  }

 private:
  WebDatabase* web_database_;
};

ACTION_P4(MakeAutofillSyncComponents, service, wd, pdm, dtc) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!BrowserThread::CurrentlyOn(BrowserThread::DB))
    return ProfileSyncFactory::SyncComponents(NULL, NULL);
  AutofillModelAssociator* model_associator =
      new AutofillModelAssociator(service, wd, pdm);
  AutofillChangeProcessor* change_processor =
      new AutofillChangeProcessor(model_associator, wd, pdm, dtc);
  return ProfileSyncFactory::SyncComponents(model_associator,
                                            change_processor);
}

ACTION_P4(MakeAutofillProfileSyncComponents, service, wd, pdm, dtc) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!BrowserThread::CurrentlyOn(BrowserThread::DB))
    return ProfileSyncFactory::SyncComponents(NULL, NULL);
  AutofillProfileModelAssociator* model_associator =
      new AutofillProfileModelAssociator(service, wd, pdm);
  AutofillProfileChangeProcessor* change_processor =
      new AutofillProfileChangeProcessor(model_associator, wd, pdm, dtc);
  return ProfileSyncFactory::SyncComponents(model_associator,
                                            change_processor);
}

class AbstractAutofillFactory {
 public:
  virtual AutofillDataTypeController* CreateDataTypeController(
    ProfileSyncFactory *factory,
      ProfileMock* profile,
      ProfileSyncService* service) = 0;
  virtual void SetExpectation(ProfileSyncFactoryMock* factory,
      ProfileSyncService* service,
      WebDatabase* wd,
      PersonalDataManager* pdm,
      DataTypeController* dtc) = 0;
  virtual ~AbstractAutofillFactory() {}
};

class AutofillEntryFactory : public AbstractAutofillFactory {
 public:
  browser_sync::AutofillDataTypeController* CreateDataTypeController(
      ProfileSyncFactory *factory,
      ProfileMock* profile,
      ProfileSyncService* service) {
    return new AutofillDataTypeController(factory,
        profile,
        service);
  }

  void SetExpectation(ProfileSyncFactoryMock* factory,
      ProfileSyncService* service,
      WebDatabase* wd,
      PersonalDataManager* pdm,
      DataTypeController* dtc) {
    EXPECT_CALL(*factory, CreateAutofillSyncComponents(_,_,_,_)).
        WillOnce(MakeAutofillSyncComponents(service, wd, pdm, dtc));
  }
};

class AutofillProfileFactory : public AbstractAutofillFactory {
 public:
  browser_sync::AutofillDataTypeController* CreateDataTypeController(
      ProfileSyncFactory *factory,
      ProfileMock* profile,
      ProfileSyncService* service) {
    return new AutofillProfileDataTypeController(factory,
        profile,
        service);
   }

  void SetExpectation(ProfileSyncFactoryMock* factory,
      ProfileSyncService* service,
      WebDatabase* wd,
      PersonalDataManager* pdm,
      DataTypeController* dtc) {
    EXPECT_CALL(*factory, CreateAutofillProfileSyncComponents(_,_,_,_)).
        WillOnce(MakeAutofillProfileSyncComponents(service, wd, pdm, dtc));
  }
};

class PersonalDataManagerMock: public PersonalDataManager {
 public:
  MOCK_CONST_METHOD0(IsDataLoaded, bool());
  MOCK_METHOD0(LoadProfiles, void());
  MOCK_METHOD0(LoadCreditCards, void());
  MOCK_METHOD0(Refresh, void());
};
template <class T> class AddAutofillTask;

class ProfileSyncServiceAutofillTest : public AbstractProfileSyncServiceTest {
 protected:
  ProfileSyncServiceAutofillTest() : db_thread_(BrowserThread::DB) {
  }

  AutofillProfileFactory profile_factory_;
  AutofillEntryFactory entry_factory_;

  AbstractAutofillFactory* GetFactory(syncable::ModelType type) {
    if (type == syncable::AUTOFILL) {
      return &entry_factory_;
    } else if (type == syncable::AUTOFILL_PROFILE) {
      return &profile_factory_;
    } else {
      NOTREACHED();
      return NULL;
    }
  }
  virtual void SetUp() {
    web_data_service_ = new WebDataServiceFake(&web_database_);
    personal_data_manager_ = new PersonalDataManagerMock();
    EXPECT_CALL(*personal_data_manager_, LoadProfiles()).Times(1);
    EXPECT_CALL(*personal_data_manager_, LoadCreditCards()).Times(1);
    personal_data_manager_->Init(&profile_);
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

  void StartSyncService(Task* task,
                        bool will_fail_association,
                        syncable::ModelType type) {
    AbstractAutofillFactory* factory = GetFactory(type);
    service_.reset(
        new TestProfileSyncService(&factory_, &profile_, "test_user", false,
                                   task));
    AutofillDataTypeController* data_type_controller =
        factory->CreateDataTypeController(&factory_,
            &profile_,
            service_.get());
   SyncBackendHostForProfileSyncTest::
       SetDefaultExpectationsForWorkerCreation(&profile_);

   factory->SetExpectation(&factory_,
       service_.get(),
       &web_database_,
       personal_data_manager_.get(),
       data_type_controller);

    EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
        WillOnce(ReturnNewDataTypeManager());

    EXPECT_CALL(profile_, GetWebDataService(_)).
        WillOnce(Return(web_data_service_.get()));

    EXPECT_CALL(profile_, GetPersonalDataManager()).
        WillRepeatedly(Return(personal_data_manager_.get()));

    EXPECT_CALL(*personal_data_manager_, IsDataLoaded()).
        WillRepeatedly(Return(true));

     // We need tokens to get the tests going
    token_service_.IssueAuthTokenForTest(
        GaiaConstants::kSyncService, "token");

    EXPECT_CALL(profile_, GetTokenService()).
        WillRepeatedly(Return(&token_service_));

    service_->set_num_expected_resumes(will_fail_association ? 0 : 1);
    service_->RegisterDataTypeController(data_type_controller);
    service_->Initialize();
    MessageLoop::current()->Run();
  }

  bool AddAutofillSyncNode(const AutofillEntry& entry) {
    sync_api::WriteTransaction trans(service_->GetUserShare());
    sync_api::ReadNode autofill_root(&trans);
    if (!autofill_root.InitByTagLookup(browser_sync::kAutofillTag))
      return false;

    sync_api::WriteNode node(&trans);
    std::string tag = AutofillModelAssociator::KeyToTag(entry.key().name(),
                                                        entry.key().value());
    if (!node.InitUniqueByCreation(syncable::AUTOFILL, autofill_root, tag))
      return false;

    AutofillChangeProcessor::WriteAutofillEntry(entry, &node);
    return true;
  }

  bool AddAutofillSyncNode(const AutoFillProfile& profile) {
    sync_api::WriteTransaction trans(service_->GetUserShare());
    sync_api::ReadNode autofill_root(&trans);
    if (!autofill_root.InitByTagLookup(browser_sync::kAutofillProfileTag))
      return false;
    sync_api::WriteNode node(&trans);
    std::string tag = profile.guid();
    if (!node.InitUniqueByCreation(syncable::AUTOFILL_PROFILE,
        autofill_root, tag))
      return false;
    AutofillProfileChangeProcessor::WriteAutofillProfile(profile, &node);
    return true;
  }

  bool GetAutofillEntriesFromSyncDB(std::vector<AutofillEntry>* entries,
                                    std::vector<AutoFillProfile>* profiles) {
    sync_api::ReadTransaction trans(service_->GetUserShare());
    sync_api::ReadNode autofill_root(&trans);
    if (!autofill_root.InitByTagLookup(browser_sync::kAutofillTag))
      return false;

    int64 child_id = autofill_root.GetFirstChildId();
    while (child_id != sync_api::kInvalidId) {
      sync_api::ReadNode child_node(&trans);
      if (!child_node.InitByIdLookup(child_id))
        return false;

      const sync_pb::AutofillSpecifics& autofill(
          child_node.GetAutofillSpecifics());
      if (autofill.has_value()) {
        AutofillKey key(UTF8ToUTF16(autofill.name()),
                        UTF8ToUTF16(autofill.value()));
        std::vector<base::Time> timestamps;
        int timestamps_count = autofill.usage_timestamp_size();
        for (int i = 0; i < timestamps_count; ++i) {
          timestamps.push_back(Time::FromInternalValue(
              autofill.usage_timestamp(i)));
        }
        entries->push_back(AutofillEntry(key, timestamps));
      } else if (autofill.has_profile()) {
        AutoFillProfile p;
        p.set_guid(autofill.profile().guid());
        AutofillProfileModelAssociator::OverwriteProfileWithServerData(&p,
            autofill.profile());
        profiles->push_back(p);
      }
      child_id = child_node.GetSuccessorId();
    }
    return true;
  }

  bool GetAutofillProfilesFromSyncDBUnderProfileNode(
      std::vector<AutoFillProfile>* profiles) {
    sync_api::ReadTransaction trans(service_->GetUserShare());
    sync_api::ReadNode autofill_root(&trans);
    if (!autofill_root.InitByTagLookup(browser_sync::kAutofillProfileTag))
      return false;

    int64 child_id = autofill_root.GetFirstChildId();
    while (child_id != sync_api::kInvalidId) {
      sync_api::ReadNode child_node(&trans);
      if (!child_node.InitByIdLookup(child_id))
        return false;

      const sync_pb::AutofillProfileSpecifics& autofill(
          child_node.GetAutofillProfileSpecifics());
        AutoFillProfile p;
        p.set_guid(autofill.guid());
        AutofillProfileModelAssociator::OverwriteProfileWithServerData(&p,
            autofill);
        profiles->push_back(p);
      child_id = child_node.GetSuccessorId();
    }
    return true;
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

  friend class AddAutofillTask<AutofillEntry>;
  friend class AddAutofillTask<AutoFillProfile>;
  friend class FakeServerUpdater;

  BrowserThread db_thread_;
  scoped_refptr<ThreadNotificationService> notification_service_;

  ProfileMock profile_;
  WebDatabaseMock web_database_;
  scoped_refptr<WebDataService> web_data_service_;
  scoped_refptr<PersonalDataManagerMock> personal_data_manager_;
};

template <class T>
class AddAutofillTask : public Task {
 public:
  AddAutofillTask(ProfileSyncServiceAutofillTest* test,
                         const std::vector<T>& entries)
      : test_(test), entries_(entries), success_(false) {
  }

  virtual void Run() {
    if (!test_->CreateRoot(GetModelType<T>()))
      return;
    for (size_t i = 0; i < entries_.size(); ++i) {
      if (!test_->AddAutofillSyncNode(entries_[i]))
        return;
    }
    success_ = true;
  }
  bool success() { return success_; }

 private:
  ProfileSyncServiceAutofillTest* test_;
  const std::vector<T>& entries_;
  bool success_;
};

// Overload write transaction to use custom NotifyTransactionComplete
static const bool kLoggingInfo = true;
class WriteTransactionTest: public WriteTransaction {
 public:
  WriteTransactionTest(const syncable::ScopedDirLookup& directory,
                       WriterTag writer, const char* source_file,
                       int line,
                       scoped_ptr<WaitableEvent> *wait_for_syncapi)
      : WriteTransaction(directory, writer, source_file, line),
        wait_for_syncapi_(wait_for_syncapi) { }

  virtual void NotifyTransactionComplete() {
    // This is where we differ. Force a thread change here, giving another
    // thread a chance to create a WriteTransaction
    (*wait_for_syncapi_)->Wait();

    WriteTransaction::NotifyTransactionComplete();
  }

 private:
  scoped_ptr<WaitableEvent> *wait_for_syncapi_;
};

// Our fake server updater. Needs the RefCountedThreadSafe inheritance so we can
// post tasks with it.
class FakeServerUpdater: public base::RefCountedThreadSafe<FakeServerUpdater> {
 public:
  FakeServerUpdater(TestProfileSyncService *service,
                    scoped_ptr<WaitableEvent> *wait_for_start,
                    scoped_ptr<WaitableEvent> *wait_for_syncapi)
      : entry_(ProfileSyncServiceAutofillTest::MakeAutofillEntry("0", "0", 0)),
        service_(service),
        wait_for_start_(wait_for_start),
        wait_for_syncapi_(wait_for_syncapi),
        is_finished_(false, false) { }

  void Update() {
    // This gets called in a modelsafeworker thread.
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));

    sync_api::UserShare* user_share = service_->GetUserShare();
    syncable::DirectoryManager* dir_manager = user_share->dir_manager.get();
    syncable::ScopedDirLookup dir(dir_manager, user_share->name);
    ASSERT_TRUE(dir.good());

    // Create autofill protobuf
    std::string tag = AutofillModelAssociator::KeyToTag(entry_.key().name(),
                                                        entry_.key().value());
    sync_pb::AutofillSpecifics new_autofill;
    new_autofill.set_name(UTF16ToUTF8(entry_.key().name()));
    new_autofill.set_value(UTF16ToUTF8(entry_.key().value()));
    const std::vector<base::Time>& ts(entry_.timestamps());
    for (std::vector<base::Time>::const_iterator timestamp = ts.begin();
         timestamp != ts.end(); ++timestamp) {
      new_autofill.add_usage_timestamp(timestamp->ToInternalValue());
    }

    sync_pb::EntitySpecifics entity_specifics;
    entity_specifics.MutableExtension(sync_pb::autofill)->
        CopyFrom(new_autofill);

    {
      // Tell main thread we've started
      (*wait_for_start_)->Signal();

      // Create write transaction.
      WriteTransactionTest trans(dir, UNITTEST, __FILE__, __LINE__,
                                 wait_for_syncapi_);

      // Create actual entry based on autofill protobuf information.
      // Simulates effects of SyncerUtil::UpdateLocalDataFromServerData
      MutableEntry parent(&trans, GET_BY_SERVER_TAG, kAutofillTag);
      MutableEntry item(&trans, CREATE, parent.Get(syncable::ID), tag);
      ASSERT_TRUE(item.good());
      item.Put(SPECIFICS, entity_specifics);
      item.Put(SERVER_SPECIFICS, entity_specifics);
      item.Put(BASE_VERSION, 1);
      syncable::Id server_item_id = service_->id_factory()->NewServerId();
      item.Put(syncable::ID, server_item_id);
      syncable::Id new_predecessor;
      ASSERT_TRUE(item.PutPredecessor(new_predecessor));
    }
    VLOG(1) << "FakeServerUpdater finishing.";
    is_finished_.Signal();
  }

  void CreateNewEntry(const AutofillEntry& entry) {
    entry_ = entry;
    scoped_ptr<Callback0::Type> c(NewCallback((FakeServerUpdater *)this,
                                              &FakeServerUpdater::Update));
    std::vector<browser_sync::ModelSafeWorker*> workers;
    service_->GetBackendForTest()->GetWorkers(&workers);

    ASSERT_FALSE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    if (!BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
         NewRunnableMethod(this, &FakeServerUpdater::Update))) {
      NOTREACHED() << "Failed to post task to the db thread.";
      return;
    }
  }

  void CreateNewEntryAndWait(const AutofillEntry& entry) {
    entry_ = entry;
    scoped_ptr<Callback0::Type> c(NewCallback((FakeServerUpdater *)this,
                                              &FakeServerUpdater::Update));
    std::vector<browser_sync::ModelSafeWorker*> workers;
    service_->GetBackendForTest()->GetWorkers(&workers);

    ASSERT_FALSE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    is_finished_.Reset();
    if (!BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
         NewRunnableMethod(this, &FakeServerUpdater::Update))) {
      NOTREACHED() << "Failed to post task to the db thread.";
      return;
    }
    is_finished_.Wait();
  }

 private:
  friend class base::RefCountedThreadSafe<FakeServerUpdater>;
  ~FakeServerUpdater() { }

  AutofillEntry entry_;
  TestProfileSyncService *service_;
  scoped_ptr<WaitableEvent> *wait_for_start_;
  scoped_ptr<WaitableEvent> *wait_for_syncapi_;
  WaitableEvent is_finished_;
  syncable::Id parent_id_;
};

// TODO(skrul): Test abort startup.
// TODO(skrul): Test processing of cloud changes.
// TODO(tim): Add autofill data type controller test, and a case to cover
//            waiting for the PersonalDataManager.
TEST_F(ProfileSyncServiceAutofillTest, FailModelAssociation) {
  // Don't create the root autofill node so startup fails.
  StartSyncService(NULL, true, syncable::AUTOFILL);
  EXPECT_TRUE(service_->unrecoverable_error_detected());
}

TEST_F(ProfileSyncServiceAutofillTest, EmptyNativeEmptySync) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateRootTask task(this, syncable::AUTOFILL);
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  EXPECT_EQ(0U, sync_entries.size());
  EXPECT_EQ(0U, sync_profiles.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeEntriesEmptySync) {
  std::vector<AutofillEntry> entries;
  entries.push_back(MakeAutofillEntry("foo", "bar", 1));
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateRootTask task(this, syncable::AUTOFILL);
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  ASSERT_EQ(1U, entries.size());
  EXPECT_TRUE(entries[0] == sync_entries[0]);
  EXPECT_EQ(0U, sync_profiles.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasProfileEmptySync) {

  std::vector<AutoFillProfile*> profiles;
  std::vector<AutoFillProfile> expected_profiles;
  // Owned by GetAutoFillProfiles caller.
  AutoFillProfile* profile0 = new AutoFillProfile;
  autofill_test::SetProfileInfoWithGuid(profile0,
      "54B3F9AA-335E-4F71-A27D-719C41564230", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  profiles.push_back(profile0);
  expected_profiles.push_back(*profile0);
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(profiles), Return(true)));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootTask task(this, syncable::AUTOFILL_PROFILE);
  StartSyncService(&task, false, syncable::AUTOFILL_PROFILE);
  ASSERT_TRUE(task.success());
  std::vector<AutoFillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(&sync_profiles));
  EXPECT_EQ(1U, sync_profiles.size());
  EXPECT_EQ(0, expected_profiles[0].Compare(sync_profiles[0]));
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
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateRootTask task(this, syncable::AUTOFILL);
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  EXPECT_EQ(2U, sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncNoMerge) {
  AutofillEntry native_entry(MakeAutofillEntry("native", "entry", 1));
  AutofillEntry sync_entry(MakeAutofillEntry("sync", "entry", 2));

  std::vector<AutofillEntry> native_entries;
  native_entries.push_back(native_entry);

  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));

  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));

  std::vector<AutofillEntry> sync_entries;
  sync_entries.push_back(sync_entry);

  AddAutofillTask<AutofillEntry> task(this, sync_entries);

  EXPECT_CALL(web_database_, UpdateAutofillEntries(ElementsAre(sync_entry))).
      WillOnce(Return(true));

  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());

  std::set<AutofillEntry> expected_entries;
  expected_entries.insert(native_entry);
  expected_entries.insert(sync_entry);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  std::set<AutofillEntry> new_sync_entries_set(new_sync_entries.begin(),
                                               new_sync_entries.end());

  EXPECT_TRUE(expected_entries == new_sync_entries_set);
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMergeEntry) {
  AutofillEntry native_entry(MakeAutofillEntry("merge", "entry", 1));
  AutofillEntry sync_entry(MakeAutofillEntry("merge", "entry", 2));
  AutofillEntry merged_entry(MakeAutofillEntry("merge", "entry", 1, 2));

  std::vector<AutofillEntry> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));

  std::vector<AutofillEntry> sync_entries;
  sync_entries.push_back(sync_entry);
  AddAutofillTask<AutofillEntry> task(this, sync_entries);

  EXPECT_CALL(web_database_, UpdateAutofillEntries(ElementsAre(merged_entry))).
      WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(merged_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMergeProfile) {
  AutoFillProfile sync_profile;
  autofill_test::SetProfileInfoWithGuid(&sync_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile* native_profile = new AutoFillProfile;
  autofill_test::SetProfileInfoWithGuid(native_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  std::vector<AutoFillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutoFillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillTask<AutoFillProfile> task(this, sync_profiles);

  EXPECT_CALL(web_database_, UpdateAutoFillProfile(_)).
      WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(&task, false, syncable::AUTOFILL_PROFILE);
  ASSERT_TRUE(task.success());

  std::vector<AutoFillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, sync_profile.Compare(new_sync_profiles[0]));
}

TEST_F(ProfileSyncServiceAutofillTest, MergeProfileWithDifferentGuid) {
  AutoFillProfile sync_profile;

  autofill_test::SetProfileInfoWithGuid(&sync_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  std::string native_guid = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
  AutoFillProfile* native_profile = new AutoFillProfile;
  autofill_test::SetProfileInfoWithGuid(native_profile,
      native_guid.c_str(), "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  std::vector<AutoFillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutoFillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillTask<AutoFillProfile> task(this, sync_profiles);

  EXPECT_CALL(web_database_, AddAutoFillProfile(_)).
      WillOnce(Return(true));
  EXPECT_CALL(web_database_, RemoveAutoFillProfile(native_guid)).
      WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(&task, false, syncable::AUTOFILL_PROFILE);
  ASSERT_TRUE(task.success());

  std::vector<AutoFillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, sync_profile.Compare(new_sync_profiles[0]));
  EXPECT_EQ(sync_profile.guid(), new_sync_profiles[0].guid());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddEntry) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootTask task(this, syncable::AUTOFILL);
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());

  AutofillEntry added_entry(MakeAutofillEntry("added", "entry", 1));
  std::vector<base::Time> timestamps(added_entry.timestamps());

  EXPECT_CALL(web_database_, GetAutofillTimestamps(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(timestamps), Return(true)));

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::ADD, added_entry.key()));
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&db_thread_));
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Source<WebDataService>(web_data_service_.get()),
                   Details<AutofillChangeList>(&changes));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(added_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddProfile) {
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootTask task(this, syncable::AUTOFILL_PROFILE);
  StartSyncService(&task, false, syncable::AUTOFILL_PROFILE);
  ASSERT_TRUE(task.success());

  AutoFillProfile added_profile;
  autofill_test::SetProfileInfoWithGuid(&added_profile,
      "D6ADA912-D374-4C0A-917D-F5C8EBE43011", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  AutofillProfileChange change(AutofillProfileChange::ADD,
      added_profile.guid(), &added_profile);
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&db_thread_));
  notifier->Notify(NotificationType::AUTOFILL_PROFILE_CHANGED,
                   Source<WebDataService>(web_data_service_.get()),
                   Details<AutofillProfileChange>(&change));

  std::vector<AutoFillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, added_profile.Compare(new_sync_profiles[0]));
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeUpdateEntry) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  CreateRootTask task(this, syncable::AUTOFILL);
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());

  AutofillEntry updated_entry(MakeAutofillEntry("my", "entry", 1, 2));
  std::vector<base::Time> timestamps(updated_entry.timestamps());

  EXPECT_CALL(web_database_, GetAutofillTimestamps(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(timestamps), Return(true)));

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::UPDATE,
                                   updated_entry.key()));
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&db_thread_));
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Source<WebDataService>(web_data_service_.get()),
                   Details<AutofillChangeList>(&changes));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(updated_entry == new_sync_entries[0]);
}


TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemoveEntry) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  CreateRootTask task(this, syncable::AUTOFILL);
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                   original_entry.key()));
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&db_thread_));
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Source<WebDataService>(web_data_service_.get()),
                   Details<AutofillChangeList>(&changes));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(0U, new_sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemoveProfile) {
  AutoFillProfile sync_profile;
  autofill_test::SetProfileInfoWithGuid(&sync_profile,
      "3BA5FA1B-1EC4-4BB3-9B57-EC92BE3C1A09", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");
  AutoFillProfile* native_profile = new AutoFillProfile;
  autofill_test::SetProfileInfoWithGuid(native_profile,
      "3BA5FA1B-1EC4-4BB3-9B57-EC92BE3C1A09", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  std::vector<AutoFillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutoFillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillTask<AutoFillProfile> task(this, sync_profiles);
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(&task, false, syncable::AUTOFILL_PROFILE);
  ASSERT_TRUE(task.success());

  AutofillProfileChange change(AutofillProfileChange::REMOVE,
                               sync_profile.guid(), NULL);
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&db_thread_));
  notifier->Notify(NotificationType::AUTOFILL_PROFILE_CHANGED,
                   Source<WebDataService>(web_data_service_.get()),
                   Details<AutofillProfileChange>(&change));

  std::vector<AutoFillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(0U, new_sync_profiles.size());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeError) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  CreateRootTask task(this, syncable::AUTOFILL);
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());

  // Inject an evil entry into the sync db to conflict with the same
  // entry added by the user.
  AutofillEntry evil_entry(MakeAutofillEntry("evil", "entry", 1));
  ASSERT_TRUE(AddAutofillSyncNode(evil_entry));

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::ADD,
                                   evil_entry.key()));
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&db_thread_));
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Source<WebDataService>(web_data_service_.get()),
                   Details<AutofillChangeList>(&changes));

  // Wait for the PPS to shut everything down and signal us.
  ProfileSyncServiceObserverMock observer;
  service_->AddObserver(&observer);
  EXPECT_CALL(observer, OnStateChanged()).WillOnce(QuitUIMessageLoop());
  MessageLoop::current()->Run();
  EXPECT_TRUE(service_->unrecoverable_error_detected());

  // Ensure future autofill notifications don't crash.
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Source<WebDataService>(web_data_service_.get()),
                   Details<AutofillChangeList>(&changes));
}

// Crashy, http://crbug.com/57884
TEST_F(ProfileSyncServiceAutofillTest, DISABLED_ServerChangeRace) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, UpdateAutofillEntries(_)).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh()).Times(3);
  CreateRootTask task(this, syncable::AUTOFILL);
  StartSyncService(&task, false, syncable::AUTOFILL);
  ASSERT_TRUE(task.success());

  // (true, false) means we have to reset after |Signal|, init to unsignaled.
  scoped_ptr<WaitableEvent> wait_for_start(new WaitableEvent(true, false));
  scoped_ptr<WaitableEvent> wait_for_syncapi(new WaitableEvent(true, false));
  scoped_refptr<FakeServerUpdater> updater(new FakeServerUpdater(
      service_.get(), &wait_for_start, &wait_for_syncapi));

  // This server side update will stall waiting for CommitWaiter.
  updater->CreateNewEntry(MakeAutofillEntry("server", "entry", 1));
  wait_for_start->Wait();

  AutofillEntry syncapi_entry(MakeAutofillEntry("syncapi", "entry", 2));
  ASSERT_TRUE(AddAutofillSyncNode(syncapi_entry));
  VLOG(1) << "Syncapi update finished.";

  // If we reach here, it means syncapi succeeded and we didn't deadlock. Yay!
  // Signal FakeServerUpdater that it can complete.
  wait_for_syncapi->Signal();

  // Make another entry to ensure nothing broke afterwards and wait for finish
  // to clean up.
  updater->CreateNewEntryAndWait(MakeAutofillEntry("server2", "entry2", 3));

  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  EXPECT_EQ(3U, sync_entries.size());
  EXPECT_EQ(0U, sync_profiles.size());
  for (size_t i = 0; i < sync_entries.size(); i++) {
    VLOG(1) << "Entry " << i << ": " << sync_entries[i].key().name()
            << ", " << sync_entries[i].key().value();
  }
}

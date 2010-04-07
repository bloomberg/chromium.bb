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
#include "chrome/browser/autofill/autofill_common_unittest.h"
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
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
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
               bool(const std::vector<AutofillEntry>&));  // NOLINT
  MOCK_METHOD1(GetAutoFillProfiles,
               bool(std::vector<AutoFillProfile*>*));  // NOLINT
  MOCK_METHOD1(UpdateAutoFillProfile,
               bool(const AutoFillProfile&));  // NOLINT
  MOCK_METHOD1(AddAutoFillProfile,
               bool(const AutoFillProfile&));  // NOLINT
  MOCK_METHOD1(RemoveAutoFillProfile,
               bool(int));  // NOLINT
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

class PersonalDataManagerMock: public PersonalDataManager {
 public:
  MOCK_CONST_METHOD0(IsDataLoaded, bool());
  MOCK_METHOD0(LoadProfiles, void());
  MOCK_METHOD0(LoadCreditCards, void());
  MOCK_METHOD0(Refresh, void());
};

ACTION_P4(MakeAutofillSyncComponents, service, wd, pdm, dtc) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  AutofillModelAssociator* model_associator =
      new AutofillModelAssociator(service, wd, pdm, dtc);
  AutofillChangeProcessor* change_processor =
      new AutofillChangeProcessor(model_associator, wd, pdm, dtc);
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
    personal_data_manager_.Init(&profile_);
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

      EXPECT_CALL(factory_, CreateAutofillSyncComponents(_, _, _, _)).
          WillOnce(MakeAutofillSyncComponents(service_.get(),
                                              &web_database_,
                                              &personal_data_manager_,
                                              data_type_controller));
      EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
          WillOnce(MakeDataTypeManager(&backend_));

      EXPECT_CALL(profile_, GetWebDataService(_)).
          WillOnce(Return(web_data_service_.get()));

      EXPECT_CALL(profile_, GetPersonalDataManager()).
          WillRepeatedly(Return(&personal_data_manager_));

      EXPECT_CALL(personal_data_manager_, IsDataLoaded()).
        WillRepeatedly(Return(true));

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
    AutofillChangeProcessor::WriteAutofillEntry(entry, &node);
  }

  void AddAutofillProfileSyncNode(const AutoFillProfile& profile) {
    sync_api::WriteTransaction trans(
        service_->backend()->GetUserShareHandle());
    sync_api::ReadNode autofill_root(&trans);
    ASSERT_TRUE(autofill_root.InitByTagLookup(browser_sync::kAutofillTag));
    sync_api::WriteNode node(&trans);
    std::string tag = AutofillModelAssociator::ProfileLabelToTag(
        profile.Label());
    ASSERT_TRUE(node.InitUniqueByCreation(syncable::AUTOFILL,
                                          autofill_root,
                                          tag));
    AutofillChangeProcessor::WriteAutofillProfile(profile, &node);
    sync_pb::AutofillSpecifics s(node.GetAutofillSpecifics());
    s.mutable_profile()->set_label(UTF16ToUTF8(profile.Label()));
    node.SetAutofillSpecifics(s);
  }

  void GetAutofillEntriesFromSyncDB(std::vector<AutofillEntry>* entries,
                                    std::vector<AutoFillProfile>* profiles) {
    sync_api::ReadTransaction trans(service_->backend()->GetUserShareHandle());
    sync_api::ReadNode autofill_root(&trans);
    ASSERT_TRUE(autofill_root.InitByTagLookup(browser_sync::kAutofillTag));

    int64 child_id = autofill_root.GetFirstChildId();
    while (child_id != sync_api::kInvalidId) {
      sync_api::ReadNode child_node(&trans);
      ASSERT_TRUE(child_node.InitByIdLookup(child_id));

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
        AutoFillProfile p(UTF8ToUTF16(autofill.profile().label()), 0);
        AutofillModelAssociator::OverwriteProfileWithServerData(&p,
            autofill.profile());
        profiles->push_back(p);
      }
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
  PersonalDataManagerMock personal_data_manager_;

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
                         const std::vector<AutofillEntry>& entries,
                         const std::vector<AutoFillProfile>& profiles)
      : test_(test), entries_(entries), profiles_(profiles) {
  }

  virtual void Run() {
    test_->CreateAutofillRoot();
    for (size_t i = 0; i < entries_.size(); ++i) {
      test_->AddAutofillSyncNode(entries_[i]);
    }
    for (size_t i = 0; i < profiles_.size(); ++i) {
      test_->AddAutofillProfileSyncNode(profiles_[i]);
    }

  }

 private:
  ProfileSyncServiceAutofillTest* test_;
  const std::vector<AutofillEntry>& entries_;
  const std::vector<AutoFillProfile>& profiles_;
};

// TODO(skrul): Test abort startup.
// TODO(skrul): Test processing of cloud changes.
// TODO(tim): Add autofill data type controller test, and a case to cover
//            waiting for the PersonalDataManager.
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
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateAutofillRootTask task(this);
  StartSyncService(&task);
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles);
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
  CreateAutofillRootTask task(this);
  StartSyncService(&task);
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles);
  ASSERT_EQ(1U, entries.size());
  EXPECT_TRUE(entries[0] == sync_entries[0]);
  EXPECT_EQ(0U, sync_profiles.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasMixedNativeEmptySync) {
  std::vector<AutofillEntry> entries;
  entries.push_back(MakeAutofillEntry("foo", "bar", 1));
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));

  std::vector<AutoFillProfile*> profiles;
  std::vector<AutoFillProfile> expected_profiles;
  // Owned by GetAutoFillProfiles caller.
  AutoFillProfile* profile0 = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  profiles.push_back(profile0);
  expected_profiles.push_back(*profile0);
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(profiles), Return(true)));
  EXPECT_CALL(personal_data_manager_, Refresh());
  SetIdleChangeProcessorExpectations();
  CreateAutofillRootTask task(this);
  StartSyncService(&task);
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles);
  ASSERT_EQ(1U, entries.size());
  EXPECT_TRUE(entries[0] == sync_entries[0]);
  EXPECT_EQ(1U, sync_profiles.size());
  EXPECT_EQ(expected_profiles[0], sync_profiles[0]);
}

bool ProfilesMatchExceptLabelImpl(AutoFillProfile p1, AutoFillProfile p2) {
  const AutoFillFieldType types[] = { NAME_FIRST,
                                      NAME_MIDDLE,
                                      NAME_LAST,
                                      EMAIL_ADDRESS,
                                      COMPANY_NAME,
                                      ADDRESS_HOME_LINE1,
                                      ADDRESS_HOME_LINE2,
                                      ADDRESS_HOME_CITY,
                                      ADDRESS_HOME_STATE,
                                      ADDRESS_HOME_ZIP,
                                      ADDRESS_HOME_COUNTRY,
                                      PHONE_HOME_NUMBER,
                                      PHONE_FAX_NUMBER };
  if (p1.Label() == p2.Label())
    return false;

  for (size_t index = 0; index < arraysize(types); ++index) {
    if (p1.GetFieldText(AutoFillType(types[index])) !=
        p2.GetFieldText(AutoFillType(types[index])))
      return false;
  }
  return true;
}

MATCHER_P(ProfileMatchesExceptLabel, profile, "") {
  return ProfilesMatchExceptLabelImpl(arg, profile);
}

TEST_F(ProfileSyncServiceAutofillTest, HasDuplicateProfileLabelsEmptySync) {
  std::vector<AutoFillProfile> expected_profiles;
  std::vector<AutoFillProfile*> profiles;
  AutoFillProfile* profile0 = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  AutoFillProfile* profile1 = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(profile1,
      "Billing", "Same", "Label", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  profiles.push_back(profile0);
  profiles.push_back(profile1);
  expected_profiles.push_back(*profile0);
  expected_profiles.push_back(*profile1);
  AutoFillProfile relabelled_profile;
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager_, Refresh());
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(profiles), Return(true)));
  EXPECT_CALL(web_database_, UpdateAutoFillProfile(
      ProfileMatchesExceptLabel(expected_profiles[1]))).
      WillOnce(DoAll(SaveArg<0>(&relabelled_profile), Return(true)));

  SetIdleChangeProcessorExpectations();
  CreateAutofillRootTask task(this);
  StartSyncService(&task);
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles);
  EXPECT_EQ(0U, sync_entries.size());
  EXPECT_EQ(2U, sync_profiles.size());
  EXPECT_EQ(expected_profiles[0], sync_profiles[1]);
  EXPECT_TRUE(ProfilesMatchExceptLabelImpl(expected_profiles[1],
                                           sync_profiles[0]));
  EXPECT_EQ(sync_profiles[0].Label(), relabelled_profile.Label());
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
  CreateAutofillRootTask task(this);
  StartSyncService(&task);
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles);
  EXPECT_EQ(2U, sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncNoMerge) {
  AutofillEntry native_entry(MakeAutofillEntry("native", "entry", 1));
  AutofillEntry sync_entry(MakeAutofillEntry("sync", "entry", 2));
  AutoFillProfile sync_profile(string16(), 0);
  autofill_unittest::SetProfileInfo(&sync_profile,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile* native_profile = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(native_profile,
      "Work", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  std::vector<AutofillEntry> native_entries;
  native_entries.push_back(native_entry);
  std::vector<AutoFillProfile*> native_profiles;
  native_profiles.push_back(native_profile);

  std::vector<AutoFillProfile> expected_profiles;
  expected_profiles.push_back(*native_profile);
  expected_profiles.push_back(sync_profile);

  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));
  std::vector<AutofillEntry> sync_entries;
  sync_entries.push_back(sync_entry);
  std::vector<AutoFillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillEntriesTask task(this, sync_entries, sync_profiles);

  AutoFillProfile to_be_added(sync_profile);
  to_be_added.set_unique_id(1);
  EXPECT_CALL(web_database_, UpdateAutofillEntries(ElementsAre(sync_entry))).
      WillOnce(Return(true));
  EXPECT_CALL(web_database_, AddAutoFillProfile(Eq(to_be_added))).
      WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager_, Refresh());
  StartSyncService(&task);

  std::set<AutofillEntry> expected_entries;
  expected_entries.insert(native_entry);
  expected_entries.insert(sync_entry);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  std::set<AutofillEntry> new_sync_entries_set(new_sync_entries.begin(),
                                               new_sync_entries.end());

  EXPECT_TRUE(expected_entries == new_sync_entries_set);
  EXPECT_EQ(2U, new_sync_profiles.size());
  EXPECT_EQ(expected_profiles[0], new_sync_profiles[0]);
  EXPECT_EQ(expected_profiles[1], new_sync_profiles[1]);
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
  std::vector<AutoFillProfile> sync_profiles;
  sync_entries.push_back(sync_entry);
  AddAutofillEntriesTask task(this, sync_entries, sync_profiles);

  EXPECT_CALL(web_database_, UpdateAutofillEntries(ElementsAre(merged_entry))).
      WillOnce(Return(true));
  StartSyncService(&task);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(merged_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMergeProfile) {
  AutoFillProfile sync_profile(string16(), 0);
  autofill_unittest::SetProfileInfo(&sync_profile,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile* native_profile = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(native_profile,
      "Billing", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  std::vector<AutoFillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)). WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillEntriesTask task(this, sync_entries, sync_profiles);

  EXPECT_CALL(web_database_, UpdateAutoFillProfile(Eq(sync_profile))).
      WillOnce(Return(true));
  EXPECT_CALL(personal_data_manager_, Refresh());
  StartSyncService(&task);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_TRUE(sync_profile == new_sync_profiles[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddEntry) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
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
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(added_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddProfile) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateAutofillRootTask task(this);
  StartSyncService(&task);

  AutoFillProfile added_profile(string16(), 0);
  autofill_unittest::SetProfileInfo(&added_profile,
      "Billing", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  AutofillProfileChange change(AutofillProfileChange::ADD,
      added_profile.Label(), &added_profile, string16());
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_PROFILE_CHANGED,
                   Details<AutofillProfileChange>(&change));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_TRUE(added_profile == new_sync_profiles[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddProfileConflict) {
  AutoFillProfile sync_profile(string16(), 0);
  autofill_unittest::SetProfileInfo(&sync_profile,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillEntriesTask task(this, sync_entries, sync_profiles);

  sync_profile.set_unique_id(1);
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, AddAutoFillProfile(Eq(sync_profile))).
              WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  StartSyncService(&task);

  AutoFillProfile added_profile(string16(), 0);
  autofill_unittest::SetProfileInfo(&added_profile,
      "Billing", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  AutofillProfileChange change(AutofillProfileChange::ADD,
      added_profile.Label(), &added_profile, string16());

  AutoFillProfile relabelled_profile;
  EXPECT_CALL(web_database_, UpdateAutoFillProfile(
      ProfileMatchesExceptLabel(added_profile))).
      WillOnce(DoAll(SaveArg<0>(&relabelled_profile), Return(true)));
  EXPECT_CALL(personal_data_manager_, Refresh());

  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_PROFILE_CHANGED,
                   Details<AutofillProfileChange>(&change));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(2U, new_sync_profiles.size());
  sync_profile.set_unique_id(0);  // The sync DB doesn't store IDs.
  EXPECT_EQ(sync_profile, new_sync_profiles[1]);
  EXPECT_TRUE(ProfilesMatchExceptLabelImpl(added_profile,
                                           new_sync_profiles[0]));
  EXPECT_EQ(new_sync_profiles[0].Label(), relabelled_profile.Label());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeUpdateEntry) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
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
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(updated_entry == new_sync_entries[0]);
}


TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeUpdateProfile) {
  AutoFillProfile* native_profile = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(native_profile,
      "Billing", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");
  std::vector<AutoFillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)). WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));
  CreateAutofillRootTask task(this);
  StartSyncService(&task);

  AutoFillProfile update_profile(string16(), 0);
  autofill_unittest::SetProfileInfo(&update_profile,
      "Billing", "Changin'", "Mah", "Namez",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  AutofillProfileChange change(AutofillProfileChange::UPDATE,
                               update_profile.Label(), &update_profile,
                               ASCIIToUTF16("Billing"));
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_PROFILE_CHANGED,
                   Details<AutofillProfileChange>(&change));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_TRUE(update_profile == new_sync_profiles[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeUpdateProfileRelabel) {
  AutoFillProfile* native_profile = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(native_profile,
      "Billing", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");
  std::vector<AutoFillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)). WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));
  CreateAutofillRootTask task(this);
  StartSyncService(&task);

  AutoFillProfile update_profile(string16(), 0);
  autofill_unittest::SetProfileInfo(&update_profile,
      "TRYIN 2 FOOL U", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  AutofillProfileChange change(AutofillProfileChange::UPDATE,
                               update_profile.Label(), &update_profile,
                               ASCIIToUTF16("Billing"));
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_PROFILE_CHANGED,
                   Details<AutofillProfileChange>(&change));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_TRUE(update_profile == new_sync_profiles[0]);
}

TEST_F(ProfileSyncServiceAutofillTest,
       ProcessUserChangeUpdateProfileRelabelConflict) {
  AutoFillProfile* native_profile = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(native_profile,
      "Billing", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");
  AutoFillProfile* native_profile2 = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(native_profile2,
      "ExistingLabel", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  std::vector<AutoFillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  native_profiles.push_back(native_profile2);
  AutoFillProfile marion(*native_profile2);
  AutoFillProfile josephine_update(*native_profile);

  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)). WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));
  CreateAutofillRootTask task(this);
  StartSyncService(&task);

  josephine_update.set_label(ASCIIToUTF16("ExistingLabel"));
  AutoFillProfile relabelled_profile;
  EXPECT_CALL(web_database_, UpdateAutoFillProfile(
      ProfileMatchesExceptLabel(josephine_update))).
      WillOnce(DoAll(SaveArg<0>(&relabelled_profile), Return(true)));
  EXPECT_CALL(personal_data_manager_, Refresh());

  AutofillProfileChange change(AutofillProfileChange::UPDATE,
                               josephine_update.Label(), &josephine_update,
                               ASCIIToUTF16("Billing"));
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_PROFILE_CHANGED,
                   Details<AutofillProfileChange>(&change));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(2U, new_sync_profiles.size());
  marion.set_unique_id(0);  // The sync DB doesn't store IDs.
  EXPECT_EQ(marion, new_sync_profiles[1]);
  EXPECT_TRUE(ProfilesMatchExceptLabelImpl(josephine_update,
                                           new_sync_profiles[0]));
  EXPECT_EQ(new_sync_profiles[0].Label(), relabelled_profile.Label());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemoveEntry) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
  CreateAutofillRootTask task(this);
  StartSyncService(&task);

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                   original_entry.key()));
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   Details<AutofillChangeList>(&changes));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(0U, new_sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemoveProfile) {
  AutoFillProfile sync_profile(string16(), 0);
  autofill_unittest::SetProfileInfo(&sync_profile,
      "Billing", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");
  AutoFillProfile* native_profile = new AutoFillProfile(string16(), 0);
  autofill_unittest::SetProfileInfo(native_profile,
      "Billing", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  std::vector<AutoFillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)). WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutofillEntry> sync_entries;
  std::vector<AutoFillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillEntriesTask task(this, sync_entries, sync_profiles);

  EXPECT_CALL(personal_data_manager_, Refresh());
  StartSyncService(&task);

  AutofillProfileChange change(AutofillProfileChange::REMOVE,
                               sync_profile.Label(), NULL, string16());
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&db_thread_);
  notifier->Notify(NotificationType::AUTOFILL_PROFILE_CHANGED,
                   Details<AutofillProfileChange>(&change));

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutoFillProfile> new_sync_profiles;
  GetAutofillEntriesFromSyncDB(&new_sync_entries, &new_sync_profiles);
  ASSERT_EQ(0U, new_sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeError) {
  EXPECT_CALL(web_database_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(web_database_, GetAutoFillProfiles(_)).WillOnce(Return(true));
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

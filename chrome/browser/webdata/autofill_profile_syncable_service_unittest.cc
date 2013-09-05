// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Property;
using autofill::AutofillProfile;
using autofill::AutofillProfileChange;
using content::BrowserThread;

// Some guids for testing.
const char kGuid1[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
const char kGuid2[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";
const char kGuid3[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44D";
const char kGuid4[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44E";
const char kHttpOrigin[] = "http://www.example.com/";
const char kHttpsOrigin[] = "https://www.example.com/";
const char kSettingsOrigin[] = "Chrome settings";

class MockAutofillProfileSyncableService
    : public AutofillProfileSyncableService {
 public:
  MockAutofillProfileSyncableService() {}
  virtual ~MockAutofillProfileSyncableService() {}

  using AutofillProfileSyncableService::DataBundle;
  using AutofillProfileSyncableService::set_sync_processor;
  using AutofillProfileSyncableService::CreateData;

  MOCK_METHOD1(LoadAutofillData, bool(std::vector<AutofillProfile*>*));
  MOCK_METHOD1(SaveChangesToWebData,
               bool(const AutofillProfileSyncableService::DataBundle&));
};

ACTION_P(CopyData, data) {
  arg0->resize(data->size());
  std::copy(data->begin(), data->end(), arg0->begin());
}

MATCHER_P(CheckSyncChanges, n_sync_changes_list, "") {
  if (arg.size() != n_sync_changes_list.size())
    return false;
  syncer::SyncChangeList::const_iterator passed, expected;
  for (passed = arg.begin(), expected = n_sync_changes_list.begin();
       passed != arg.end() && expected != n_sync_changes_list.end();
       ++passed, ++expected) {
    DCHECK(passed->IsValid());
    if (passed->change_type() != expected->change_type())
      return false;
    if (passed->sync_data().GetSpecifics().autofill_profile().guid() !=
        expected->sync_data().GetSpecifics().autofill_profile().guid()) {
      return false;
    }
  }
  return true;
}

MATCHER_P(DataBundleCheck, n_bundle, "") {
  if ((arg.profiles_to_delete.size() != n_bundle.profiles_to_delete.size()) ||
      (arg.profiles_to_update.size() != n_bundle.profiles_to_update.size()) ||
      (arg.profiles_to_add.size() != n_bundle.profiles_to_add.size()))
    return false;
  for (size_t i = 0; i < arg.profiles_to_delete.size(); ++i) {
    if (arg.profiles_to_delete[i] != n_bundle.profiles_to_delete[i])
      return false;
  }
  for (size_t i = 0; i < arg.profiles_to_update.size(); ++i) {
    if (*arg.profiles_to_update[i] != *n_bundle.profiles_to_update[i])
      return false;
  }
  for (size_t i = 0; i < arg.profiles_to_add.size(); ++i) {
    if (*arg.profiles_to_add[i] != *n_bundle.profiles_to_add[i])
      return false;
  }
  return true;
}

class MockSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() {}
  virtual ~MockSyncChangeProcessor() {}

  MOCK_METHOD2(ProcessSyncChanges,
               syncer::SyncError(const tracked_objects::Location&,
                                 const syncer::SyncChangeList&));
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE { return syncer::SyncDataList(); }
};

class TestSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestSyncChangeProcessor() {}
  virtual ~TestSyncChangeProcessor() {}

  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& location,
      const syncer::SyncChangeList& changes) OVERRIDE {
    changes_ = changes;
    return syncer::SyncError();
  }

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE {
    return syncer::SyncDataList();
  }

  const syncer::SyncChangeList& changes() { return changes_; }

 private:
  syncer::SyncChangeList changes_;
};

class AutofillProfileSyncableServiceTest : public testing::Test {
 public:
  AutofillProfileSyncableServiceTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      db_thread_(BrowserThread::DB, &message_loop_) {}

  virtual void SetUp() OVERRIDE {
    sync_processor_.reset(new MockSyncChangeProcessor);
  }

  // Wrapper around AutofillProfileSyncableService::MergeDataAndStartSyncing()
  // that also verifies expectations.
  void MergeDataAndStartSyncing(
      const std::vector<AutofillProfile*>& profiles_from_web_db,
      const syncer::SyncDataList& data_list,
      const MockAutofillProfileSyncableService::DataBundle& expected_bundle,
      const syncer::SyncChangeList& expected_change_list) {
    EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
        .Times(1)
        .WillOnce(DoAll(CopyData(&profiles_from_web_db), Return(true)));
    EXPECT_CALL(autofill_syncable_service_,
                SaveChangesToWebData(DataBundleCheck(expected_bundle)))
        .Times(1)
        .WillOnce(Return(true));
    ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
        .WillByDefault(Return(syncer::SyncError()));
    EXPECT_CALL(*sync_processor_,
                ProcessSyncChanges(_, CheckSyncChanges(expected_change_list)))
        .Times(1)
        .WillOnce(Return(syncer::SyncError()));

    // Takes ownership of sync_processor_.
    autofill_syncable_service_.MergeDataAndStartSyncing(
        syncer::AUTOFILL_PROFILE, data_list,
        sync_processor_.PassAs<syncer::SyncChangeProcessor>(),
        scoped_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
  }

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  MockAutofillProfileSyncableService autofill_syncable_service_;
  scoped_ptr<MockSyncChangeProcessor> sync_processor_;
};

TEST_F(AutofillProfileSyncableServiceTest, MergeDataAndStartSyncing) {
  std::vector<AutofillProfile*> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;
  std::string origin_present1 = kHttpOrigin;
  std::string origin_present2 = std::string();
  std::string origin_synced1 = kHttpsOrigin;
  std::string origin_synced2 = kSettingsOrigin;

  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present1, origin_present1));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::NAME_FIRST, UTF8ToUTF16("John"));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("1 1st st"));
  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present2, origin_present2));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::NAME_FIRST, UTF8ToUTF16("Tom"));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("2 2nd st"));

  syncer::SyncDataList data_list;
  AutofillProfile profile1(guid_synced1, origin_synced1);
  profile1.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("Jane"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2, origin_synced2);
  profile2.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("Harry"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));
  // This one will have the name and origin updated.
  AutofillProfile profile3(guid_present2, origin_synced2);
  profile3.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("Tom Doe"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile3));

  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.front())));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_add.push_back(&profile1);
  expected_bundle.profiles_to_add.push_back(&profile2);
  expected_bundle.profiles_to_update.push_back(&profile3);

  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, MergeIdenticalProfiles) {
  std::vector<AutofillProfile*> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;
  std::string origin_present1 = kHttpOrigin;
  std::string origin_present2 = kSettingsOrigin;
  std::string origin_synced1 = kHttpsOrigin;
  std::string origin_synced2 = kHttpsOrigin;

  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present1, origin_present1));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::NAME_FIRST, UTF8ToUTF16("John"));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("1 1st st"));
  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present2, origin_present2));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::NAME_FIRST, UTF8ToUTF16("Tom"));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("2 2nd st"));

  // The synced profiles are identical to the local ones, except that the guids
  // are different.
  syncer::SyncDataList data_list;
  AutofillProfile profile1(guid_synced1, origin_synced1);
  profile1.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("John"));
  profile1.SetRawInfo(autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("1 1st st"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2, origin_synced2);
  profile2.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("Tom"));
  profile2.SetRawInfo(autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("2 2nd st"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));

  AutofillProfile expected_profile(profile2);
  expected_profile.set_origin(kSettingsOrigin);
  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_UPDATE,
                         MockAutofillProfileSyncableService::CreateData(
                             expected_profile)));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present1);
  expected_bundle.profiles_to_delete.push_back(guid_present2);
  expected_bundle.profiles_to_add.push_back(&profile1);
  expected_bundle.profiles_to_add.push_back(&expected_profile);

  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, MergeSimilarProfiles) {
  std::vector<AutofillProfile*> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;
  std::string origin_present1 = kHttpOrigin;
  std::string origin_present2 = kSettingsOrigin;
  std::string origin_synced1 = kHttpsOrigin;
  std::string origin_synced2 = kHttpsOrigin;

  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present1, origin_present1));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::NAME_FIRST, UTF8ToUTF16("John"));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("1 1st st"));
  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present2, origin_present2));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::NAME_FIRST, UTF8ToUTF16("Tom"));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("2 2nd st"));

  // The synced profiles are identical to the local ones, except that the guids
  // are different.
  syncer::SyncDataList data_list;
  AutofillProfile profile1(guid_synced1, origin_synced1);
  profile1.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("John"));
  profile1.SetRawInfo(autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("1 1st st"));
  profile1.SetRawInfo(autofill::COMPANY_NAME, UTF8ToUTF16("Frobbers, Inc."));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2, origin_synced2);
  profile2.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("Tom"));
  profile2.SetRawInfo(autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("2 2nd st"));
  profile2.SetRawInfo(autofill::COMPANY_NAME, UTF8ToUTF16("Fizzbang, LLC."));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));

  // The first profile should have its origin updated.
  // The second profile should remain as-is, because an unverified profile
  // should never overwrite a verified one.
  AutofillProfile expected_profile(profile1);
  expected_profile.set_origin(origin_present1);
  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.back())));
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_UPDATE,
                         MockAutofillProfileSyncableService::CreateData(
                             expected_profile)));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present1);
  expected_bundle.profiles_to_add.push_back(&expected_profile);
  expected_bundle.profiles_to_add.push_back(&profile2);

  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Ensure that no Sync events are generated to fill in missing origins from Sync
// with explicitly present empty ones.  This ensures that the migration to add
// origins to profiles does not generate lots of needless Sync updates.
TEST_F(AutofillProfileSyncableServiceTest, MergeDataEmptyOrigins) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Create a profile with an empty origin.
  AutofillProfile profile(kGuid1, std::string());
  profile.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("John"));
  profile.SetRawInfo(autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("1 1st st"));

  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Create a Sync profile identical to |profile|, except with no origin set.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_line1("1 1st st");
  EXPECT_FALSE(autofill_specifics->has_origin());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
      .Times(1)
      .WillOnce(DoAll(CopyData(&profiles_from_web_db), Return(true)));
  EXPECT_CALL(autofill_syncable_service_,
              SaveChangesToWebData(DataBundleCheck(expected_bundle)))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*sync_processor_, ProcessSyncChanges(_, _)).Times(0);

  // Takes ownership of sync_processor_.
  autofill_syncable_service_.MergeDataAndStartSyncing(
      syncer::AUTOFILL_PROFILE, data_list,
      sync_processor_.PassAs<syncer::SyncChangeProcessor>(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, GetAllSyncData) {
  std::vector<AutofillProfile*> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;

  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present1, kHttpOrigin));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::NAME_FIRST, UTF8ToUTF16("John"));
  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present2, kHttpsOrigin));
  profiles_from_web_db.back()->SetRawInfo(
      autofill::NAME_FIRST, UTF8ToUTF16("Jane"));

  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.front())));
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.back())));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  syncer::SyncDataList data_list;
  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);

  syncer::SyncDataList data =
      autofill_syncable_service_.GetAllSyncData(syncer::AUTOFILL_PROFILE);

  ASSERT_EQ(2U, data.size());
  EXPECT_EQ(guid_present1, data[0].GetSpecifics().autofill_profile().guid());
  EXPECT_EQ(guid_present2, data[1].GetSpecifics().autofill_profile().guid());
  EXPECT_EQ(kHttpOrigin, data[0].GetSpecifics().autofill_profile().origin());
  EXPECT_EQ(kHttpsOrigin, data[1].GetSpecifics().autofill_profile().origin());

  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, ProcessSyncChanges) {
  std::vector<AutofillProfile *> profiles_from_web_db;
  std::string guid_present = kGuid1;
  std::string guid_synced = kGuid2;

  syncer::SyncChangeList change_list;
  AutofillProfile profile(guid_synced, kHttpOrigin);
  profile.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("Jane"));
  change_list.push_back(
      syncer::SyncChange(
          FROM_HERE,
          syncer::SyncChange::ACTION_ADD,
          MockAutofillProfileSyncableService::CreateData(profile)));
  AutofillProfile empty_profile(guid_present, kHttpsOrigin);
  change_list.push_back(
      syncer::SyncChange(
          FROM_HERE,
          syncer::SyncChange::ACTION_DELETE,
          MockAutofillProfileSyncableService::CreateData(empty_profile)));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present);
  expected_bundle.profiles_to_add.push_back(&profile);

  EXPECT_CALL(autofill_syncable_service_, SaveChangesToWebData(
              DataBundleCheck(expected_bundle)))
      .Times(1)
      .WillOnce(Return(true));

  autofill_syncable_service_.set_sync_processor(sync_processor_.release());
  syncer::SyncError error = autofill_syncable_service_.ProcessSyncChanges(
      FROM_HERE, change_list);

  EXPECT_FALSE(error.IsSet());
}

TEST_F(AutofillProfileSyncableServiceTest, AutofillProfileAdded) {
  // Will be owned by the syncable service.  Keep a reference available here for
  // verifying test expectations.
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.SetRawInfo(autofill::NAME_FIRST, UTF8ToUTF16("Jane"));
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuid1, &profile);
  autofill_syncable_service_.AutofillProfileChanged(change);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, result.change_type());

  sync_pb::AutofillProfileSpecifics specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(kGuid1, specifics.guid());
  EXPECT_EQ(kHttpsOrigin, specifics.origin());
  EXPECT_THAT(specifics.name_first(), testing::ElementsAre("Jane"));
}

TEST_F(AutofillProfileSyncableServiceTest, AutofillProfileDeleted) {
  // Will be owned by the syncable service.  Keep a reference available here for
  // verifying test expectations.
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  AutofillProfileChange change(AutofillProfileChange::REMOVE, kGuid2, NULL);
  autofill_syncable_service_.AutofillProfileChanged(change);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, result.change_type());
  sync_pb::AutofillProfileSpecifics specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(kGuid2, specifics.guid());
}

TEST_F(AutofillProfileSyncableServiceTest, UpdateField) {
  AutofillProfile profile(kGuid1, kSettingsOrigin);
  std::string company1 = "A Company";
  std::string company2 = "Another Company";
  profile.SetRawInfo(autofill::COMPANY_NAME, UTF8ToUTF16(company1));
  EXPECT_FALSE(AutofillProfileSyncableService::UpdateField(
      autofill::COMPANY_NAME, company1, &profile));
  EXPECT_EQ(profile.GetRawInfo(autofill::COMPANY_NAME), UTF8ToUTF16(company1));
  EXPECT_TRUE(AutofillProfileSyncableService::UpdateField(
      autofill::COMPANY_NAME, company2, &profile));
  EXPECT_EQ(profile.GetRawInfo(autofill::COMPANY_NAME), UTF8ToUTF16(company2));
  EXPECT_FALSE(AutofillProfileSyncableService::UpdateField(
      autofill::COMPANY_NAME, company2, &profile));
  EXPECT_EQ(profile.GetRawInfo(autofill::COMPANY_NAME), UTF8ToUTF16(company2));
}

TEST_F(AutofillProfileSyncableServiceTest, UpdateMultivaluedField) {
  AutofillProfile profile(kGuid1, kHttpsOrigin);

  std::vector<string16> values;
  values.push_back(UTF8ToUTF16("1@1.com"));
  values.push_back(UTF8ToUTF16("2@1.com"));
  profile.SetRawMultiInfo(autofill::EMAIL_ADDRESS, values);

  ::google::protobuf::RepeatedPtrField<std::string> specifics_fields;
  specifics_fields.AddAllocated(new std::string("2@1.com"));
  specifics_fields.AddAllocated(new std::string("3@1.com"));

  EXPECT_TRUE(AutofillProfileSyncableService::UpdateMultivaluedField(
      autofill::EMAIL_ADDRESS, specifics_fields, &profile));
  profile.GetRawMultiInfo(autofill::EMAIL_ADDRESS, &values);
  ASSERT_TRUE(values.size() == 2);
  EXPECT_EQ(values[0], UTF8ToUTF16("2@1.com"));
  EXPECT_EQ(values[1], UTF8ToUTF16("3@1.com"));

  EXPECT_FALSE(AutofillProfileSyncableService::UpdateMultivaluedField(
      autofill::EMAIL_ADDRESS, specifics_fields, &profile));
  profile.GetRawMultiInfo(autofill::EMAIL_ADDRESS, &values);
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0], UTF8ToUTF16("2@1.com"));
  EXPECT_EQ(values[1], UTF8ToUTF16("3@1.com"));
  EXPECT_TRUE(AutofillProfileSyncableService::UpdateMultivaluedField(
      autofill::EMAIL_ADDRESS,
      ::google::protobuf::RepeatedPtrField<std::string>(),
      &profile));
  profile.GetRawMultiInfo(autofill::EMAIL_ADDRESS, &values);
  ASSERT_EQ(values.size(), 1U);  // Always have at least an empty string.
  EXPECT_EQ(values[0], UTF8ToUTF16(""));
}

TEST_F(AutofillProfileSyncableServiceTest, MergeProfile) {
  AutofillProfile profile1(kGuid1, kHttpOrigin);
  profile1.SetRawInfo(
      autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("111 First St."));

  std::vector<string16> values;
  values.push_back(UTF8ToUTF16("1@1.com"));
  values.push_back(UTF8ToUTF16("2@1.com"));
  profile1.SetRawMultiInfo(autofill::EMAIL_ADDRESS, values);

  AutofillProfile profile2(kGuid2, kHttpsOrigin);
  profile2.SetRawInfo(
      autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("111 First St."));

  // |values| now is [ "1@1.com", "2@1.com", "3@1.com" ].
  values.push_back(UTF8ToUTF16("3@1.com"));
  profile2.SetRawMultiInfo(autofill::EMAIL_ADDRESS, values);

  values.clear();
  values.push_back(UTF8ToUTF16("John"));
  profile1.SetRawMultiInfo(autofill::NAME_FIRST, values);
  values.push_back(UTF8ToUTF16("Jane"));
  profile2.SetRawMultiInfo(autofill::NAME_FIRST, values);

  values.clear();
  values.push_back(UTF8ToUTF16("Doe"));
  profile1.SetRawMultiInfo(autofill::NAME_LAST, values);
  values.push_back(UTF8ToUTF16("Other"));
  profile2.SetRawMultiInfo(autofill::NAME_LAST, values);

  values.clear();
  values.push_back(UTF8ToUTF16("650234567"));
  profile2.SetRawMultiInfo(autofill::PHONE_HOME_WHOLE_NUMBER, values);

  EXPECT_FALSE(AutofillProfileSyncableService::MergeProfile(profile2,
                                                            &profile1,
                                                            "en-US"));

  profile1.GetRawMultiInfo(autofill::NAME_FIRST, &values);
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0], UTF8ToUTF16("John"));
  EXPECT_EQ(values[1], UTF8ToUTF16("Jane"));

  profile1.GetRawMultiInfo(autofill::NAME_LAST, &values);
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0], UTF8ToUTF16("Doe"));
  EXPECT_EQ(values[1], UTF8ToUTF16("Other"));

  profile1.GetRawMultiInfo(autofill::EMAIL_ADDRESS, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0], UTF8ToUTF16("1@1.com"));
  EXPECT_EQ(values[1], UTF8ToUTF16("2@1.com"));
  EXPECT_EQ(values[2], UTF8ToUTF16("3@1.com"));

  profile1.GetRawMultiInfo(autofill::PHONE_HOME_WHOLE_NUMBER, &values);
  ASSERT_EQ(values.size(), 1U);
  EXPECT_EQ(values[0], UTF8ToUTF16("650234567"));

  EXPECT_EQ(profile2.origin(), profile1.origin());

  AutofillProfile profile3(kGuid3, kHttpOrigin);
  profile3.SetRawInfo(
      autofill::ADDRESS_HOME_LINE1, UTF8ToUTF16("111 First St."));

  values.clear();
  values.push_back(UTF8ToUTF16("Jane"));
  profile3.SetRawMultiInfo(autofill::NAME_FIRST, values);

  values.clear();
  values.push_back(UTF8ToUTF16("Doe"));
  profile3.SetRawMultiInfo(autofill::NAME_LAST, values);

  EXPECT_TRUE(AutofillProfileSyncableService::MergeProfile(profile3,
                                                           &profile1,
                                                            "en-US"));

  profile1.GetRawMultiInfo(autofill::NAME_FIRST, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0], UTF8ToUTF16("John"));
  EXPECT_EQ(values[1], UTF8ToUTF16("Jane"));
  EXPECT_EQ(values[2], UTF8ToUTF16("Jane"));

  profile1.GetRawMultiInfo(autofill::NAME_LAST, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0], UTF8ToUTF16("Doe"));
  EXPECT_EQ(values[1], UTF8ToUTF16("Other"));
  EXPECT_EQ(values[2], UTF8ToUTF16("Doe"));

  // Middle name should have three entries as well.
  profile1.GetRawMultiInfo(autofill::NAME_MIDDLE, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_TRUE(values[0].empty());
  EXPECT_TRUE(values[1].empty());
  EXPECT_TRUE(values[2].empty());

  profile1.GetRawMultiInfo(autofill::EMAIL_ADDRESS, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0], UTF8ToUTF16("1@1.com"));
  EXPECT_EQ(values[1], UTF8ToUTF16("2@1.com"));
  EXPECT_EQ(values[2], UTF8ToUTF16("3@1.com"));

  profile1.GetRawMultiInfo(autofill::PHONE_HOME_WHOLE_NUMBER, &values);
  ASSERT_EQ(values.size(), 1U);
  EXPECT_EQ(values[0], UTF8ToUTF16("650234567"));
}

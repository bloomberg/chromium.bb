// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_profile_syncable_service.h"
#include "content/test/test_browser_thread.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/syncable/syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Property;
using content::BrowserThread;
class AutofillProfile;

// Some guids for testing.
std::string kGuid1 = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
std::string kGuid2 = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";
std::string kGuid3 = "EDC609ED-7EEE-4F27-B00C-423242A9C44D";
std::string kGuid4 = "EDC609ED-7EEE-4F27-B00C-423242A9C44E";

class MockAutofillProfileSyncableService
    : public AutofillProfileSyncableService {
 public:
  MockAutofillProfileSyncableService() {
  }
  virtual ~MockAutofillProfileSyncableService() {}

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
  SyncChangeList::const_iterator passed, expected;
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
      (arg.profiles_to_add.size() != n_bundle.profiles_to_add.size())) {
    return false;
  }
  for (size_t i = 0; i < arg.profiles_to_delete.size(); ++i) {
    if (arg.profiles_to_delete[i] != n_bundle.profiles_to_delete[i])
      return false;
  }
  for (size_t i = 0; i < arg.profiles_to_update.size(); ++i) {
    if (arg.profiles_to_update[i]->guid() !=
        n_bundle.profiles_to_update[i]->guid()) {
      return false;
    }
  }
  for (size_t i = 0; i < arg.profiles_to_add.size(); ++i) {
    if (arg.profiles_to_add[i]->Compare(*n_bundle.profiles_to_add[i]))
      return false;
  }
  return true;
}

class MockSyncChangeProcessor : public SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() {}
  virtual ~MockSyncChangeProcessor() {}

  MOCK_METHOD2(ProcessSyncChanges,
               SyncError(const tracked_objects::Location&,
                         const SyncChangeList&));
};

class AutofillProfileSyncableServiceTest : public testing::Test {
 public:
  AutofillProfileSyncableServiceTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      db_thread_(BrowserThread::DB, &message_loop_) {}

  virtual void SetUp() OVERRIDE {
    sync_processor_.reset(new MockSyncChangeProcessor);
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  MockAutofillProfileSyncableService autofill_syncable_service_;
  scoped_ptr<MockSyncChangeProcessor> sync_processor_;
};

TEST_F(AutofillProfileSyncableServiceTest, MergeDataAndStartSyncing) {
  std::vector<AutofillProfile *> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;

  profiles_from_web_db.push_back(new AutofillProfile(guid_present1));
  profiles_from_web_db.back()->SetInfo(NAME_FIRST, UTF8ToUTF16("John"));
  profiles_from_web_db.back()->SetInfo(ADDRESS_HOME_LINE1,
                                       UTF8ToUTF16("1 1st st"));
  profiles_from_web_db.push_back(new AutofillProfile(guid_present2));
  profiles_from_web_db.back()->SetInfo(NAME_FIRST, UTF8ToUTF16("Tom"));
  profiles_from_web_db.back()->SetInfo(ADDRESS_HOME_LINE1,
                                       UTF8ToUTF16("2 2nd st"));

  SyncDataList data_list;
  AutofillProfile profile1(guid_synced1);
  profile1.SetInfo(NAME_FIRST, UTF8ToUTF16("Jane"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2);
  profile2.SetInfo(NAME_FIRST, UTF8ToUTF16("Harry"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));
  // This one will have the name updated.
  AutofillProfile profile3(guid_present2);
  profile3.SetInfo(NAME_FIRST, UTF8ToUTF16("Tom Doe"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile3));

  SyncChangeList expected_change_list;
  expected_change_list.push_back(
      SyncChange(SyncChange::ACTION_ADD,
                 AutofillProfileSyncableService::CreateData(
                     (*profiles_from_web_db.front()))));

  AutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_add.push_back(&profile1);
  expected_bundle.profiles_to_add.push_back(&profile2);
  expected_bundle.profiles_to_update.push_back(&profile3);

  EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
      .Times(1)
      .WillOnce(DoAll(CopyData(&profiles_from_web_db), Return(true)));
  EXPECT_CALL(autofill_syncable_service_,
              SaveChangesToWebData(DataBundleCheck(expected_bundle)))
      .Times(1)
      .WillOnce(Return(true));
  ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
      .WillByDefault(Return(SyncError()));
  EXPECT_CALL(*sync_processor_,
              ProcessSyncChanges(_, CheckSyncChanges(expected_change_list)))
      .Times(1)
      .WillOnce(Return(SyncError()));

  // Takes ownership of sync_processor_.
  autofill_syncable_service_.MergeDataAndStartSyncing(
      syncable::AUTOFILL_PROFILE, data_list,
      sync_processor_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));
  autofill_syncable_service_.StopSyncing(syncable::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, GetAllSyncData) {
  std::vector<AutofillProfile *> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;

  profiles_from_web_db.push_back(new AutofillProfile(guid_present1));
  profiles_from_web_db.back()->SetInfo(NAME_FIRST, UTF8ToUTF16("John"));
  profiles_from_web_db.push_back(new AutofillProfile(guid_present2));
  profiles_from_web_db.back()->SetInfo(NAME_FIRST, UTF8ToUTF16("Jane"));

  EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
      .Times(1)
      .WillOnce(DoAll(CopyData(&profiles_from_web_db), Return(true)));
  EXPECT_CALL(autofill_syncable_service_, SaveChangesToWebData(_))
      .Times(1)
      .WillOnce(Return(true));
  ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
      .WillByDefault(Return(SyncError()));
  EXPECT_CALL(*sync_processor_,
              ProcessSyncChanges(_, Property(&SyncChangeList::size, Eq(2U))))
      .Times(1)
      .WillOnce(Return(SyncError()));

  SyncDataList data_list;
  // Takes ownership of sync_processor_.
  autofill_syncable_service_.MergeDataAndStartSyncing(
      syncable::AUTOFILL_PROFILE, data_list,
      sync_processor_.PassAs<SyncChangeProcessor>(),
      scoped_ptr<SyncErrorFactory>(new SyncErrorFactoryMock()));

  SyncDataList data =
      autofill_syncable_service_.GetAllSyncData(syncable::AUTOFILL_PROFILE);

  EXPECT_EQ(2U, data.size());
  EXPECT_EQ(guid_present1, data.front().GetSpecifics()
      .autofill_profile().guid());
  EXPECT_EQ(guid_present2, data.back().GetSpecifics()
                               .autofill_profile().guid());
}

TEST_F(AutofillProfileSyncableServiceTest, ProcessSyncChanges) {
  std::vector<AutofillProfile *> profiles_from_web_db;
  std::string guid_present = kGuid1;
  std::string guid_synced = kGuid2;

  SyncChangeList change_list;
  AutofillProfile profile(guid_synced);
  profile.SetInfo(NAME_FIRST, UTF8ToUTF16("Jane"));
  change_list.push_back(
      SyncChange(SyncChange::ACTION_ADD,
      AutofillProfileSyncableService::CreateData(profile)));
  AutofillProfile empty_profile(guid_present);
  change_list.push_back(
      SyncChange(SyncChange::ACTION_DELETE,
                 AutofillProfileSyncableService::CreateData(empty_profile)));

  AutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present);
  expected_bundle.profiles_to_add.push_back(&profile);

  EXPECT_CALL(autofill_syncable_service_, SaveChangesToWebData(
              DataBundleCheck(expected_bundle)))
      .Times(1)
      .WillOnce(Return(true));

  autofill_syncable_service_.set_sync_processor(sync_processor_.release());
  SyncError error = autofill_syncable_service_.ProcessSyncChanges(
      FROM_HERE, change_list);

  EXPECT_FALSE(error.IsSet());
}

TEST_F(AutofillProfileSyncableServiceTest, ActOnChange) {
  AutofillProfile profile(kGuid1);
  profile.SetInfo(NAME_FIRST, UTF8ToUTF16("Jane"));
  AutofillProfileChange change1(AutofillProfileChange::ADD, kGuid1, &profile);
  AutofillProfileChange change2(AutofillProfileChange::REMOVE, kGuid2, NULL);
  ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
      .WillByDefault(Return(SyncError(FROM_HERE, std::string("an error"),
                                      syncable::AUTOFILL_PROFILE)));
  EXPECT_CALL(*sync_processor_, ProcessSyncChanges(_, _)).Times(2);

  autofill_syncable_service_.set_sync_processor(sync_processor_.release());
  autofill_syncable_service_.ActOnChange(change1);
  autofill_syncable_service_.ActOnChange(change2);
}

TEST_F(AutofillProfileSyncableServiceTest, UpdateField) {
  AutofillProfile profile(kGuid1);
  std::string company1 = "A Company";
  std::string company2 = "Another Company";
  profile.SetInfo(COMPANY_NAME, UTF8ToUTF16(company1));
  EXPECT_FALSE(AutofillProfileSyncableService::UpdateField(
      COMPANY_NAME, company1, &profile));
  EXPECT_EQ(profile.GetInfo(COMPANY_NAME), UTF8ToUTF16(company1));
  EXPECT_TRUE(AutofillProfileSyncableService::UpdateField(
      COMPANY_NAME, company2, &profile));
  EXPECT_EQ(profile.GetInfo(COMPANY_NAME), UTF8ToUTF16(company2));
  EXPECT_FALSE(AutofillProfileSyncableService::UpdateField(
      COMPANY_NAME, company2, &profile));
  EXPECT_EQ(profile.GetInfo(COMPANY_NAME), UTF8ToUTF16(company2));
}

TEST_F(AutofillProfileSyncableServiceTest, UpdateMultivaluedField) {
  AutofillProfile profile(kGuid1);

  std::vector<string16> values;
  values.push_back(UTF8ToUTF16("1@1.com"));
  values.push_back(UTF8ToUTF16("2@1.com"));
  profile.SetMultiInfo(EMAIL_ADDRESS, values);

  ::google::protobuf::RepeatedPtrField<std::string> specifics_fields;
  specifics_fields.AddAllocated(new std::string("2@1.com"));
  specifics_fields.AddAllocated(new std::string("3@1.com"));

  EXPECT_TRUE(AutofillProfileSyncableService::UpdateMultivaluedField(
      EMAIL_ADDRESS, specifics_fields, &profile));
  profile.GetMultiInfo(EMAIL_ADDRESS, &values);
  ASSERT_TRUE(values.size() == 2);
  EXPECT_EQ(values[0], UTF8ToUTF16("2@1.com"));
  EXPECT_EQ(values[1], UTF8ToUTF16("3@1.com"));

  EXPECT_FALSE(AutofillProfileSyncableService::UpdateMultivaluedField(
      EMAIL_ADDRESS, specifics_fields, &profile));
  profile.GetMultiInfo(EMAIL_ADDRESS, &values);
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0], UTF8ToUTF16("2@1.com"));
  EXPECT_EQ(values[1], UTF8ToUTF16("3@1.com"));
  EXPECT_TRUE(AutofillProfileSyncableService::UpdateMultivaluedField(
      EMAIL_ADDRESS, ::google::protobuf::RepeatedPtrField<std::string>(),
      &profile));
  profile.GetMultiInfo(EMAIL_ADDRESS, &values);
  ASSERT_EQ(values.size(), 1U);  // Always have at least an empty string.
  EXPECT_EQ(values[0], UTF8ToUTF16(""));
}

TEST_F(AutofillProfileSyncableServiceTest, MergeProfile) {
  AutofillProfile profile1(kGuid1);
  profile1.SetInfo(ADDRESS_HOME_LINE1, UTF8ToUTF16("111 First St."));

  std::vector<string16> values;
  values.push_back(UTF8ToUTF16("1@1.com"));
  values.push_back(UTF8ToUTF16("2@1.com"));
  profile1.SetMultiInfo(EMAIL_ADDRESS, values);

  AutofillProfile profile2(kGuid2);
  profile2.SetInfo(ADDRESS_HOME_LINE1, UTF8ToUTF16("111 First St."));

  // |values| now is [ "1@1.com", "2@1.com", "3@1.com" ].
  values.push_back(UTF8ToUTF16("3@1.com"));
  profile2.SetMultiInfo(EMAIL_ADDRESS, values);

  values.clear();
  values.push_back(UTF8ToUTF16("John"));
  profile1.SetMultiInfo(NAME_FIRST, values);
  values.push_back(UTF8ToUTF16("Jane"));
  profile2.SetMultiInfo(NAME_FIRST, values);

  values.clear();
  values.push_back(UTF8ToUTF16("Doe"));
  profile1.SetMultiInfo(NAME_LAST, values);
  values.push_back(UTF8ToUTF16("Other"));
  profile2.SetMultiInfo(NAME_LAST, values);

  values.clear();
  values.push_back(UTF8ToUTF16("650234567"));
  profile2.SetMultiInfo(PHONE_HOME_WHOLE_NUMBER, values);

  EXPECT_FALSE(AutofillProfileSyncableService::MergeProfile(profile2,
                                                            &profile1));

  profile1.GetMultiInfo(NAME_FIRST, &values);
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0], UTF8ToUTF16("John"));
  EXPECT_EQ(values[1], UTF8ToUTF16("Jane"));

  profile1.GetMultiInfo(NAME_LAST, &values);
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0], UTF8ToUTF16("Doe"));
  EXPECT_EQ(values[1], UTF8ToUTF16("Other"));

  profile1.GetMultiInfo(EMAIL_ADDRESS, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0], UTF8ToUTF16("1@1.com"));
  EXPECT_EQ(values[1], UTF8ToUTF16("2@1.com"));
  EXPECT_EQ(values[2], UTF8ToUTF16("3@1.com"));

  profile1.GetMultiInfo(PHONE_HOME_WHOLE_NUMBER, &values);
  ASSERT_EQ(values.size(), 1U);
  EXPECT_EQ(values[0], UTF8ToUTF16("650234567"));

  AutofillProfile profile3(kGuid3);
  profile3.SetInfo(ADDRESS_HOME_LINE1, UTF8ToUTF16("111 First St."));

  values.clear();
  values.push_back(UTF8ToUTF16("Jane"));
  profile3.SetMultiInfo(NAME_FIRST, values);

  values.clear();
  values.push_back(UTF8ToUTF16("Doe"));
  profile3.SetMultiInfo(NAME_LAST, values);

  EXPECT_TRUE(AutofillProfileSyncableService::MergeProfile(profile3,
                                                           &profile1));

  profile1.GetMultiInfo(NAME_FIRST, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0], UTF8ToUTF16("John"));
  EXPECT_EQ(values[1], UTF8ToUTF16("Jane"));
  EXPECT_EQ(values[2], UTF8ToUTF16("Jane"));

  profile1.GetMultiInfo(NAME_LAST, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0], UTF8ToUTF16("Doe"));
  EXPECT_EQ(values[1], UTF8ToUTF16("Other"));
  EXPECT_EQ(values[2], UTF8ToUTF16("Doe"));

  // Middle name should have three entries as well.
  profile1.GetMultiInfo(NAME_MIDDLE, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_TRUE(values[0].empty());
  EXPECT_TRUE(values[1].empty());
  EXPECT_TRUE(values[2].empty());

  profile1.GetMultiInfo(EMAIL_ADDRESS, &values);
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0], UTF8ToUTF16("1@1.com"));
  EXPECT_EQ(values[1], UTF8ToUTF16("2@1.com"));
  EXPECT_EQ(values[2], UTF8ToUTF16("3@1.com"));

  profile1.GetMultiInfo(PHONE_HOME_WHOLE_NUMBER, &values);
  ASSERT_EQ(values.size(), 1U);
  EXPECT_EQ(values[0], UTF8ToUTF16("650234567"));
}

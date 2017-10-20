// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_syncable_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::SyncChange;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using testing::AnyNumber;
using testing::DoAll;
using testing::ElementsAre;
using testing::IgnoreResult;
using testing::Invoke;
using testing::IsEmpty;
using testing::Matches;
using testing::Return;
using testing::SetArgPointee;
using testing::UnorderedElementsAre;
using testing::_;

namespace password_manager {

// Defined in the implementation file corresponding to this test.
syncer::SyncData SyncDataFromPassword(const autofill::PasswordForm& password);
autofill::PasswordForm PasswordFromSpecifics(
    const sync_pb::PasswordSpecificsData& password);
std::string MakePasswordSyncTag(const sync_pb::PasswordSpecificsData& password);
std::string MakePasswordSyncTag(const autofill::PasswordForm& password);

namespace {

// PasswordForm values for tests.
constexpr autofill::PasswordForm::Type kArbitraryType =
    autofill::PasswordForm::TYPE_GENERATED;
constexpr char kAndroidAutofillRealm[] = "android://hash@com.magisto";
constexpr char kAndroidCorrectRealm[] = "android://hash@com.magisto/";
constexpr char kIconUrl[] = "https://fb.com/Icon";
constexpr char kDisplayName[] = "Agent Smith";
constexpr char kFederationUrl[] = "https://fb.com/";
constexpr char kPassword[] = "abcdef";
constexpr char kSignonRealm[] = "abc";
constexpr char kSignonRealm2[] = "def";
constexpr char kSignonRealm3[] = "xyz";
constexpr int kTimesUsed = 5;
constexpr char kUsername[] = "godzilla";

typedef std::vector<SyncChange> SyncChangeList;

const sync_pb::PasswordSpecificsData& GetPasswordSpecifics(
    const syncer::SyncData& sync_data) {
  return sync_data.GetSpecifics().password().client_only_encrypted_data();
}

MATCHER_P(HasDateSynced, time, "") {
  return arg.date_synced == time;
}

MATCHER_P(PasswordIs, form, "") {
  sync_pb::PasswordSpecificsData actual_password =
      GetPasswordSpecifics(SyncDataFromPassword(arg));
  sync_pb::PasswordSpecificsData expected_password =
      GetPasswordSpecifics(SyncDataFromPassword(form));
  if (expected_password.scheme() == actual_password.scheme() &&
      expected_password.signon_realm() == actual_password.signon_realm() &&
      expected_password.origin() == actual_password.origin() &&
      expected_password.action() == actual_password.action() &&
      expected_password.username_element() ==
          actual_password.username_element() &&
      expected_password.password_element() ==
          actual_password.password_element() &&
      expected_password.username_value() == actual_password.username_value() &&
      expected_password.password_value() == actual_password.password_value() &&
      expected_password.preferred() == actual_password.preferred() &&
      expected_password.date_created() == actual_password.date_created() &&
      expected_password.blacklisted() == actual_password.blacklisted() &&
      expected_password.type() == actual_password.type() &&
      expected_password.times_used() == actual_password.times_used() &&
      expected_password.display_name() == actual_password.display_name() &&
      expected_password.avatar_url() == actual_password.avatar_url() &&
      expected_password.federation_url() == actual_password.federation_url())
    return true;

  *result_listener << "Password protobuf does not match; expected:\n"
                   << form << '\n'
                   << "actual:" << '\n'
                   << arg;
  return false;
}

MATCHER_P2(SyncChangeIs, change_type, password, "") {
  const SyncData& data = arg.sync_data();
  autofill::PasswordForm form =
      PasswordFromSpecifics(GetPasswordSpecifics(data));
  return (arg.change_type() == change_type &&
          syncer::SyncDataLocal(data).GetTag() ==
              MakePasswordSyncTag(password) &&
          (change_type == SyncChange::ACTION_DELETE ||
           Matches(PasswordIs(password))(form)));
}

// The argument is std::vector<autofill::PasswordForm*>*. The caller is
// responsible for the lifetime of all the password forms.
ACTION_P(AppendForm, form) {
  arg0->push_back(std::make_unique<autofill::PasswordForm>(form));
  return true;
}

// The argument is std::vector<autofill::PasswordForm*>*. The caller is
// responsible for the lifetime of all the password forms.
ACTION_P(AppendForms, forms) {
  for (const autofill::PasswordForm& form : forms)
    arg0->push_back(std::make_unique<autofill::PasswordForm>(form));
  return true;
}

// Creates a sync data consisting of password specifics. The sign on realm is
// set to |signon_realm|.
SyncData CreateSyncData(const std::string& signon_realm) {
  sync_pb::EntitySpecifics password_data;
  sync_pb::PasswordSpecificsData* password_specifics =
      password_data.mutable_password()->mutable_client_only_encrypted_data();
  password_specifics->set_signon_realm(signon_realm);
  password_specifics->set_type(autofill::PasswordForm::TYPE_GENERATED);
  password_specifics->set_times_used(3);
  password_specifics->set_display_name("Mr. X");
  password_specifics->set_avatar_url("https://accounts.google.com/Icon");
  password_specifics->set_federation_url("https://google.com");
  password_specifics->set_username_value("kingkong");
  password_specifics->set_password_value("sicrit");

  std::string tag = MakePasswordSyncTag(*password_specifics);
  return syncer::SyncData::CreateLocalData(tag, tag, password_data);
}

SyncChange CreateSyncChange(const autofill::PasswordForm& password,
                            SyncChange::SyncChangeType type) {
  SyncData data = SyncDataFromPassword(password);
  return SyncChange(FROM_HERE, type, data);
}

// Mock implementation of SyncChangeProcessor.
class MockSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() {}

  MOCK_METHOD2(ProcessSyncChanges,
               SyncError(const base::Location&, const SyncChangeList& list));
  SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    NOTREACHED();
    return SyncDataList();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSyncChangeProcessor);
};

// Convenience wrapper around a PasswordSyncableService and PasswordStore
// pair.
class PasswordSyncableServiceWrapper {
 public:
  PasswordSyncableServiceWrapper() {
    password_store_ = new testing::StrictMock<MockPasswordStore>;
    password_store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr);
    service_.reset(
        new PasswordSyncableService(password_store_->GetSyncInterface()));
    auto clock = std::make_unique<base::SimpleTestClock>();
    clock->SetNow(time());
    service_->set_clock(std::move(clock));
    ON_CALL(*password_store_, AddLoginImpl(HasDateSynced(time())))
        .WillByDefault(Return(PasswordStoreChangeList()));
    ON_CALL(*password_store_, RemoveLoginImpl(_))
        .WillByDefault(Return(PasswordStoreChangeList()));
    ON_CALL(*password_store_, UpdateLoginImpl(HasDateSynced(time())))
        .WillByDefault(Return(PasswordStoreChangeList()));
    EXPECT_CALL(*password_store(), NotifyLoginsChanged(_)).Times(AnyNumber());
  }

  ~PasswordSyncableServiceWrapper() { password_store_->ShutdownOnUIThread(); }

  MockPasswordStore* password_store() { return password_store_.get(); }

  PasswordSyncableService* service() { return service_.get(); }

  static base::Time time() { return base::Time::FromInternalValue(100000); }

  // Returnes the scoped_ptr to |service_| thus NULLing out it.
  std::unique_ptr<syncer::SyncChangeProcessor> ReleaseSyncableService() {
    return std::move(service_);
  }

 private:
  scoped_refptr<MockPasswordStore> password_store_;
  std::unique_ptr<PasswordSyncableService> service_;

  DISALLOW_COPY_AND_ASSIGN(PasswordSyncableServiceWrapper);
};

class PasswordSyncableServiceTest : public testing::Test {
 public:
  PasswordSyncableServiceTest()
      : processor_(new testing::StrictMock<MockSyncChangeProcessor>) {
    ON_CALL(*processor_, ProcessSyncChanges(_, _))
        .WillByDefault(Return(SyncError()));
  }
  MockPasswordStore* password_store() { return wrapper_.password_store(); }
  PasswordSyncableService* service() { return wrapper_.service(); }

  MOCK_METHOD1(StartSyncFlare, void(syncer::ModelType));

 protected:
  std::unique_ptr<MockSyncChangeProcessor> processor_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  PasswordSyncableServiceWrapper wrapper_;
};

// Both sync and password db have data that are not present in the other.
TEST_F(PasswordSyncableServiceTest, AdditionsInBoth) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);

  SyncDataList list;
  list.push_back(CreateSyncData(kSignonRealm2));
  autofill::PasswordForm new_from_sync =
      PasswordFromSpecifics(GetPasswordSpecifics(list.back()));

  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));
  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(new_from_sync)));
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, ElementsAre(
      SyncChangeIs(SyncChange::ACTION_ADD, form))));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, list, std::move(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>());
}

// Sync has data that is not present in the password db.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInSync) {
  SyncDataList list;
  list.push_back(CreateSyncData(kSignonRealm));
  autofill::PasswordForm new_from_sync =
      PasswordFromSpecifics(GetPasswordSpecifics(list.back()));

  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));
  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(new_from_sync)));
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, list, std::move(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>());
}

// Passwords db has data that is not present in sync.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInPasswordStore) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.times_used = kTimesUsed;
  form.type = kArbitraryType;
  form.display_name = base::ASCIIToUTF16(kDisplayName);
  form.icon_url = GURL(kIconUrl);
  form.federation_origin = url::Origin::Create(GURL(kFederationUrl));
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));

  EXPECT_CALL(*processor_, ProcessSyncChanges(_, ElementsAre(
      SyncChangeIs(SyncChange::ACTION_ADD, form))));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(), std::move(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync contain the same data.
TEST_F(PasswordSyncableServiceTest, BothInSync) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.times_used = kTimesUsed;
  form.type = kArbitraryType;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));

  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(1, SyncDataFromPassword(form)),
      std::move(processor_), std::unique_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync have the same data but they need to be merged
// as some fields of the data differ.
TEST_F(PasswordSyncableServiceTest, Merge) {
  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  form1.action = GURL("http://pie.com");
  form1.date_created = base::Time::Now();
  form1.preferred = true;
  form1.username_value = base::ASCIIToUTF16(kUsername);
  form1.password_value = base::ASCIIToUTF16(kPassword);

  autofill::PasswordForm form2(form1);
  form2.preferred = false;
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form1));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));
  EXPECT_CALL(*password_store(), UpdateLoginImpl(PasswordIs(form2)));
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(1, SyncDataFromPassword(form2)),
      std::move(processor_), std::unique_ptr<syncer::SyncErrorFactory>());
}

// Initiate sync due to local DB changes.
TEST_F(PasswordSyncableServiceTest, PasswordStoreChanges) {
  // Save the reference to the processor because |processor_| is NULL after
  // MergeDataAndStartSyncing().
  MockSyncChangeProcessor& weak_processor = *processor_;
  EXPECT_CALL(weak_processor, ProcessSyncChanges(_, IsEmpty()));
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));
  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, SyncDataList(), std::move(processor_),
      std::unique_ptr<syncer::SyncErrorFactory>());

  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  autofill::PasswordForm form2;
  form2.signon_realm = kSignonRealm2;
  autofill::PasswordForm form3;
  form3.signon_realm = kSignonRealm3;

  SyncChangeList sync_list;
  sync_list.push_back(CreateSyncChange(form1, SyncChange::ACTION_ADD));
  sync_list.push_back(CreateSyncChange(form2, SyncChange::ACTION_UPDATE));
  sync_list.push_back(CreateSyncChange(form3, SyncChange::ACTION_DELETE));

  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form1));
  list.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form2));
  list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form3));
  EXPECT_CALL(weak_processor, ProcessSyncChanges(_, ElementsAre(
      SyncChangeIs(SyncChange::ACTION_ADD, form1),
      SyncChangeIs(SyncChange::ACTION_UPDATE, form2),
      SyncChangeIs(SyncChange::ACTION_DELETE, form3))));
  service()->ActOnPasswordStoreChanges(list);
}

// Process all types of changes from sync.
TEST_F(PasswordSyncableServiceTest, ProcessSyncChanges) {
  autofill::PasswordForm updated_form;
  updated_form.signon_realm = kSignonRealm;
  updated_form.action = GURL("http://foo.com");
  updated_form.date_created = base::Time::Now();
  updated_form.username_value = base::ASCIIToUTF16(kUsername);
  updated_form.password_value = base::ASCIIToUTF16(kPassword);
  autofill::PasswordForm deleted_form;
  deleted_form.signon_realm = kSignonRealm2;
  deleted_form.action = GURL("http://bar.com");
  deleted_form.blacklisted_by_user = true;

  SyncData add_data = CreateSyncData(kSignonRealm3);
  autofill::PasswordForm new_from_sync =
      PasswordFromSpecifics(GetPasswordSpecifics(add_data));

  SyncChangeList list;
  list.push_back(
      SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD, add_data));
  list.push_back(
      CreateSyncChange(updated_form, syncer::SyncChange::ACTION_UPDATE));
  list.push_back(
      CreateSyncChange(deleted_form, syncer::SyncChange::ACTION_DELETE));
  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(new_from_sync)));
  EXPECT_CALL(*password_store(), UpdateLoginImpl(PasswordIs(updated_form)));
  EXPECT_CALL(*password_store(), RemoveLoginImpl(PasswordIs(deleted_form)));
  service()->ProcessSyncChanges(FROM_HERE, list);
}

// Retrives sync data from the model.
TEST_F(PasswordSyncableServiceTest, GetAllSyncData) {
  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  form1.action = GURL("http://foo.com");
  form1.times_used = kTimesUsed;
  form1.type = kArbitraryType;
  form1.display_name = base::ASCIIToUTF16(kDisplayName);
  form1.icon_url = GURL(kIconUrl);
  form1.federation_origin = url::Origin::Create(GURL(kFederationUrl));
  form1.username_value = base::ASCIIToUTF16(kUsername);
  form1.password_value = base::ASCIIToUTF16(kPassword);
  autofill::PasswordForm form2;
  form2.signon_realm = kSignonRealm2;
  form2.action = GURL("http://bar.com");
  form2.blacklisted_by_user = true;
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form1));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_))
      .WillOnce(AppendForm(form2));

  SyncDataList actual_list = service()->GetAllSyncData(syncer::PASSWORDS);
  std::vector<autofill::PasswordForm> actual_form_list;
  for (SyncDataList::iterator it = actual_list.begin(); it != actual_list.end();
       ++it) {
    actual_form_list.push_back(
        PasswordFromSpecifics(GetPasswordSpecifics(*it)));
  }
  EXPECT_THAT(actual_form_list,
              UnorderedElementsAre(PasswordIs(form1), PasswordIs(form2)));
}

// Creates 2 PasswordSyncableService instances, merges the content of the first
// one to the second one and back.
TEST_F(PasswordSyncableServiceTest, MergeDataAndPushBack) {
  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  form1.action = GURL("http://foo.com");
  form1.username_value = base::ASCIIToUTF16(kUsername);
  form1.password_value = base::ASCIIToUTF16(kPassword);

  PasswordSyncableServiceWrapper other_service_wrapper;
  autofill::PasswordForm form2;
  form2.signon_realm = kSignonRealm2;
  form2.action = GURL("http://bar.com");
  form2.username_value = base::ASCIIToUTF16(kUsername);
  form2.password_value = base::ASCIIToUTF16(kPassword);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form1));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));
  EXPECT_CALL(*other_service_wrapper.password_store(),
              FillAutofillableLogins(_)).WillOnce(AppendForm(form2));
  EXPECT_CALL(*other_service_wrapper.password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));

  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(form2)));
  EXPECT_CALL(*other_service_wrapper.password_store(),
              AddLoginImpl(PasswordIs(form1)));

  syncer::SyncDataList other_service_data =
      other_service_wrapper.service()->GetAllSyncData(syncer::PASSWORDS);
  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, other_service_data,
      other_service_wrapper.ReleaseSyncableService(),
      std::unique_ptr<syncer::SyncErrorFactory>());
}

// Calls ActOnPasswordStoreChanges without SyncChangeProcessor. StartSyncFlare
// should be called.
TEST_F(PasswordSyncableServiceTest, StartSyncFlare) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));

  // No flare and no SyncChangeProcessor, the call shouldn't crash.
  service()->ActOnPasswordStoreChanges(list);

  // Set the flare. It should be called as there is no SyncChangeProcessor.
  service()->InjectStartSyncFlare(base::Bind(
      &PasswordSyncableServiceTest::StartSyncFlare, base::Unretained(this)));
  EXPECT_CALL(*this, StartSyncFlare(syncer::PASSWORDS));
  service()->ActOnPasswordStoreChanges(list);
}

// Start syncing with an error. Subsequent password store updates shouldn't be
// propagated to Sync.
TEST_F(PasswordSyncableServiceTest, FailedReadFromPasswordStore) {
  std::unique_ptr<syncer::SyncErrorFactoryMock> error_factory(
      new syncer::SyncErrorFactoryMock);
  syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                          "Failed to get passwords from store.",
                          syncer::PASSWORDS);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(false));
  EXPECT_CALL(*error_factory, CreateAndUploadError(_, _))
      .WillOnce(Return(error));
  // ActOnPasswordStoreChanges() below shouldn't generate any changes for Sync.
  // |processor_| will be destroyed in MergeDataAndStartSyncing().
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, _)).Times(0);
  syncer::SyncMergeResult result = service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, syncer::SyncDataList(), std::move(processor_),
      std::move(error_factory));
  EXPECT_TRUE(result.error().IsSet());

  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  service()->ActOnPasswordStoreChanges(list);
}

// Start syncing with an error in ProcessSyncChanges. Subsequent password store
// updates shouldn't be propagated to Sync.
TEST_F(PasswordSyncableServiceTest, FailedProcessSyncChanges) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16(kPassword);
  std::unique_ptr<syncer::SyncErrorFactoryMock> error_factory(
      new syncer::SyncErrorFactoryMock);
  syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                          "There is a problem", syncer::PASSWORDS);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_)).WillOnce(Return(true));

  // ActOnPasswordStoreChanges() below shouldn't generate any changes for Sync.
  // |processor_| will be destroyed in MergeDataAndStartSyncing().
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, _))
      .Times(1)
      .WillOnce(Return(error));
  syncer::SyncMergeResult result = service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, syncer::SyncDataList(), std::move(processor_),
      std::move(error_factory));
  EXPECT_TRUE(result.error().IsSet());

  form.signon_realm = kSignonRealm2;
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  service()->ActOnPasswordStoreChanges(list);
}

// Serialize and deserialize empty federation_origin and make sure it's an empty
// string.
TEST_F(PasswordSyncableServiceTest, SerializeEmptyFederation) {
  autofill::PasswordForm form;
  EXPECT_TRUE(form.federation_origin.unique());
  syncer::SyncData data = SyncDataFromPassword(form);
  const sync_pb::PasswordSpecificsData& specifics = GetPasswordSpecifics(data);
  EXPECT_TRUE(specifics.has_federation_url());
  EXPECT_EQ(std::string(), specifics.federation_url());

  // Deserialize back.
  form = PasswordFromSpecifics(specifics);
  EXPECT_TRUE(form.federation_origin.unique());

  // Make sure that the Origins uploaded incorrectly are still deserialized
  // correctly.
  // crbug.com/593380.
  sync_pb::PasswordSpecificsData specifics1;
  specifics1.set_federation_url("null");
  form = PasswordFromSpecifics(specifics1);
  EXPECT_TRUE(form.federation_origin.unique());
}

// Serialize empty PasswordForm and make sure the Sync representation is
// matching the expectations
TEST_F(PasswordSyncableServiceTest, SerializeEmptyPasswordForm) {
  autofill::PasswordForm form;
  syncer::SyncData data = SyncDataFromPassword(form);
  const sync_pb::PasswordSpecificsData& specifics = GetPasswordSpecifics(data);
  EXPECT_TRUE(specifics.has_scheme());
  EXPECT_EQ(0, specifics.scheme());
  EXPECT_TRUE(specifics.has_signon_realm());
  EXPECT_EQ("", specifics.signon_realm());
  EXPECT_TRUE(specifics.has_origin());
  EXPECT_EQ("", specifics.origin());
  EXPECT_TRUE(specifics.has_action());
  EXPECT_EQ("", specifics.action());
  EXPECT_TRUE(specifics.has_username_element());
  EXPECT_EQ("", specifics.username_element());
  EXPECT_TRUE(specifics.has_username_value());
  EXPECT_EQ("", specifics.username_value());
  EXPECT_TRUE(specifics.has_password_element());
  EXPECT_EQ("", specifics.password_element());
  EXPECT_TRUE(specifics.has_password_value());
  EXPECT_EQ("", specifics.password_value());
  EXPECT_TRUE(specifics.has_preferred());
  EXPECT_FALSE(specifics.preferred());
  EXPECT_TRUE(specifics.has_date_created());
  EXPECT_EQ(0, specifics.date_created());
  EXPECT_TRUE(specifics.has_blacklisted());
  EXPECT_FALSE(specifics.blacklisted());
  EXPECT_TRUE(specifics.has_type());
  EXPECT_EQ(0, specifics.type());
  EXPECT_TRUE(specifics.has_times_used());
  EXPECT_EQ(0, specifics.times_used());
  EXPECT_TRUE(specifics.has_display_name());
  EXPECT_EQ("", specifics.display_name());
  EXPECT_TRUE(specifics.has_avatar_url());
  EXPECT_EQ("", specifics.avatar_url());
  EXPECT_TRUE(specifics.has_federation_url());
  EXPECT_EQ("", specifics.federation_url());
}

// Serialize a PasswordForm with non-default member values and make sure the
// Sync representation is matching the expectations.
TEST_F(PasswordSyncableServiceTest, SerializeNonEmptyPasswordForm) {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::SCHEME_USERNAME_ONLY;
  form.signon_realm = "http://google.com/";
  form.origin = GURL("https://google.com/origin");
  form.action = GURL("https://google.com/action");
  form.username_element = base::ASCIIToUTF16("username_element");
  form.username_value = base::ASCIIToUTF16("god@google.com");
  form.password_element = base::ASCIIToUTF16("password_element");
  form.password_value = base::ASCIIToUTF16("!@#$%^&*()");
  form.preferred = true;
  form.date_created = base::Time::FromInternalValue(100);
  form.blacklisted_by_user = true;
  form.type = autofill::PasswordForm::TYPE_LAST;
  form.times_used = 11;
  form.display_name = base::ASCIIToUTF16("Great Peter");
  form.icon_url = GURL("https://google.com/icon");
  form.federation_origin = url::Origin::Create(GURL("https://google.com/"));

  syncer::SyncData data = SyncDataFromPassword(form);
  const sync_pb::PasswordSpecificsData& specifics = GetPasswordSpecifics(data);
  EXPECT_TRUE(specifics.has_scheme());
  EXPECT_EQ(autofill::PasswordForm::SCHEME_USERNAME_ONLY, specifics.scheme());
  EXPECT_TRUE(specifics.has_signon_realm());
  EXPECT_EQ("http://google.com/", specifics.signon_realm());
  EXPECT_TRUE(specifics.has_origin());
  EXPECT_EQ("https://google.com/origin", specifics.origin());
  EXPECT_TRUE(specifics.has_action());
  EXPECT_EQ("https://google.com/action", specifics.action());
  EXPECT_TRUE(specifics.has_username_element());
  EXPECT_EQ("username_element", specifics.username_element());
  EXPECT_TRUE(specifics.has_username_value());
  EXPECT_EQ("god@google.com", specifics.username_value());
  EXPECT_TRUE(specifics.has_password_element());
  EXPECT_EQ("password_element", specifics.password_element());
  EXPECT_TRUE(specifics.has_password_value());
  EXPECT_EQ("!@#$%^&*()", specifics.password_value());
  EXPECT_TRUE(specifics.has_preferred());
  EXPECT_TRUE(specifics.preferred());
  EXPECT_TRUE(specifics.has_date_created());
  EXPECT_EQ(100, specifics.date_created());
  EXPECT_TRUE(specifics.has_blacklisted());
  EXPECT_TRUE(specifics.blacklisted());
  EXPECT_TRUE(specifics.has_type());
  EXPECT_EQ(autofill::PasswordForm::TYPE_LAST, specifics.type());
  EXPECT_TRUE(specifics.has_times_used());
  EXPECT_EQ(11, specifics.times_used());
  EXPECT_TRUE(specifics.has_display_name());
  EXPECT_EQ("Great Peter", specifics.display_name());
  EXPECT_TRUE(specifics.has_avatar_url());
  EXPECT_EQ("https://google.com/icon", specifics.avatar_url());
  EXPECT_TRUE(specifics.has_federation_url());
  EXPECT_EQ("https://google.com", specifics.federation_url());
}

// Tests for Android Autofill credentials. Those are saved in the wrong format
// without trailing '/'. Nevertheless, password store should always contain the
// correct values.
class PasswordSyncableServiceAndroidAutofillTest : public testing::Test {
 public:
  PasswordSyncableServiceAndroidAutofillTest() = default;

  static PasswordFormData android_incorrect(double creation_time) {
    PasswordFormData data = {autofill::PasswordForm::SCHEME_HTML,
                             kAndroidAutofillRealm,
                             kAndroidAutofillRealm,
                             "",
                             L"",
                             L"",
                             L"",
                             L"username_value_1",
                             L"11111",
                             true,
                             creation_time};
    return data;
  }

  static PasswordFormData android_correct(double creation_time) {
    PasswordFormData data = {autofill::PasswordForm::SCHEME_HTML,
                             kAndroidCorrectRealm,
                             kAndroidCorrectRealm,
                             "",
                             L"",
                             L"",
                             L"",
                             L"username_value_1",
                             L"22222",
                             true,
                             creation_time};
    return data;
  }

  static PasswordFormData android_incorrect2(double creation_time) {
    PasswordFormData data = {autofill::PasswordForm::SCHEME_HTML,
                             kAndroidAutofillRealm,
                             kAndroidAutofillRealm,
                             "",
                             L"",
                             L"",
                             L"",
                             L"username_value_1",
                             L"33333",
                             false,
                             creation_time};
    return data;
  }

  static PasswordFormData android_correct2(double creation_time) {
    PasswordFormData data = {autofill::PasswordForm::SCHEME_HTML,
                             kAndroidCorrectRealm,
                             kAndroidCorrectRealm,
                             "",
                             L"",
                             L"",
                             L"",
                             L"username_value_1",
                             L"444444",
                             false,
                             creation_time};
    return data;
  }

  static autofill::PasswordForm FormWithCorrectTag(PasswordFormData data) {
    autofill::PasswordForm form = *FillPasswordFormWithData(data);
    form.signon_realm = kAndroidCorrectRealm;
    form.origin = GURL(kAndroidCorrectRealm);
    form.date_synced = PasswordSyncableServiceWrapper::time();
    return form;
  }

  static autofill::PasswordForm FormWithAndroidAutofillTag(
      PasswordFormData data) {
    autofill::PasswordForm form = *FillPasswordFormWithData(data);
    form.signon_realm = kAndroidAutofillRealm;
    form.origin = GURL(kAndroidAutofillRealm);
    form.date_synced = PasswordSyncableServiceWrapper::time();
    return form;
  }

  // Transforms |val| into |count| numbers from 1 to |count| inclusive.
  static std::vector<unsigned> ExtractTimestamps(unsigned val, unsigned count) {
    std::vector<unsigned> result;
    for (unsigned i = 0; i < count; ++i) {
      result.push_back(val % count + 1);
      val /= count;
    }
    return result;
  }

  static testing::Message FormDataMessage(const std::string& prefix,
                                          const PasswordFormData* data) {
    testing::Message message;
    message << prefix;
    if (data)
      message << *FillPasswordFormWithData(*data);
    else
      message << "NULL";
    return message;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(PasswordSyncableServiceAndroidAutofillTest, FourWayMerge) {
  for (unsigned val = 0; val < 4 * 4 * 4 * 4; ++val) {
    // Generate 4 creation timestamps for all the entries.
    std::vector<unsigned> dates = ExtractTimestamps(val, 4);
    ASSERT_EQ(4u, dates.size());
    const unsigned latest = *std::max_element(dates.begin(), dates.end());
    // Sync correct, Sync Android autofill, local correct, local incorrect.
    const PasswordFormData data[4] = {
        android_correct(dates[0]), android_incorrect(dates[1]),
        android_correct2(dates[2]), android_incorrect2(dates[3])};
    const PasswordFormData* latest_data =
        std::find_if(data, data + 4, [latest](const PasswordFormData& data) {
          return data.creation_time == latest;
        });
    ASSERT_TRUE(latest_data);
    std::vector<autofill::PasswordForm> expected_sync_updates;
    if (latest_data != &data[0])
      expected_sync_updates.push_back(FormWithCorrectTag(*latest_data));
    if (latest_data != &data[1])
      expected_sync_updates.push_back(FormWithAndroidAutofillTag(*latest_data));
    autofill::PasswordForm local_correct = *FillPasswordFormWithData(data[2]);
    autofill::PasswordForm local_incorrect = *FillPasswordFormWithData(data[3]);
    syncer::SyncData sync_correct =
        SyncDataFromPassword(*FillPasswordFormWithData(data[0]));
    syncer::SyncData sync_incorrect =
        SyncDataFromPassword(*FillPasswordFormWithData(data[1]));

    SCOPED_TRACE(*FillPasswordFormWithData(data[0]));
    SCOPED_TRACE(*FillPasswordFormWithData(data[1]));
    SCOPED_TRACE(*FillPasswordFormWithData(data[2]));
    SCOPED_TRACE(*FillPasswordFormWithData(data[3]));

    for (bool correct_sync_first : {true, false}) {
      auto wrapper = std::make_unique<PasswordSyncableServiceWrapper>();
      auto processor =
          std::make_unique<testing::StrictMock<MockSyncChangeProcessor>>();

      std::vector<autofill::PasswordForm> stored_forms = {local_correct,
                                                          local_incorrect};
      EXPECT_CALL(*wrapper->password_store(), FillAutofillableLogins(_))
          .WillOnce(AppendForms(stored_forms));
      EXPECT_CALL(*wrapper->password_store(), FillBlacklistLogins(_))
          .WillOnce(Return(true));
      if (latest_data != &data[2]) {
        EXPECT_CALL(*wrapper->password_store(),
                    UpdateLoginImpl(FormWithCorrectTag(*latest_data)));
      }
      if (latest_data != &data[3]) {
        EXPECT_CALL(*wrapper->password_store(),
                    UpdateLoginImpl(FormWithAndroidAutofillTag(*latest_data)));
      }

      if (expected_sync_updates.size() == 1) {
        EXPECT_CALL(*processor,
                    ProcessSyncChanges(_, ElementsAre(SyncChangeIs(
                                              SyncChange::ACTION_UPDATE,
                                              expected_sync_updates[0]))));
      } else {
        EXPECT_CALL(
            *processor,
            ProcessSyncChanges(_, UnorderedElementsAre(
                                      SyncChangeIs(SyncChange::ACTION_UPDATE,
                                                   expected_sync_updates[0]),
                                      SyncChangeIs(SyncChange::ACTION_UPDATE,
                                                   expected_sync_updates[1]))));
      }

      SyncDataList sync_list = {sync_correct, sync_incorrect};
      if (!correct_sync_first)
        std::swap(sync_list[0], sync_list[1]);
      wrapper->service()->MergeDataAndStartSyncing(
          syncer::PASSWORDS, sync_list, std::move(processor),
          std::unique_ptr<syncer::SyncErrorFactory>());
      wrapper.reset();
      // Wait til PasswordStore is destroy end therefore all the expectations on
      // it are checked.
      scoped_task_environment_.RunUntilIdle();
    }
  }
}

TEST_F(PasswordSyncableServiceAndroidAutofillTest, ThreeWayMerge) {
  for (int j = 0; j < 4; ++j) {
    // Whether the entry exists: Sync correct, Sync Android autofill,
    // local correct, local incorrect.
    bool entry_present[4] = {true, true, true, true};
    entry_present[j] = false;
    for (unsigned val = 0; val < 3 * 3 * 3; ++val) {
      // Generate 3 creation timestamps for all the entries.
      std::vector<unsigned> dates = ExtractTimestamps(val, 3);
      ASSERT_EQ(3u, dates.size());
      const unsigned latest = *std::max_element(dates.begin(), dates.end());

      // Sync correct, Sync Android autofill, local correct, local incorrect.
      std::vector<std::unique_ptr<PasswordFormData>> data;
      int date_index = 0;
      data.push_back(entry_present[0]
                         ? std::make_unique<PasswordFormData>(
                               android_correct(dates[date_index++]))
                         : nullptr);
      data.push_back(entry_present[1]
                         ? std::make_unique<PasswordFormData>(
                               android_incorrect(dates[date_index++]))
                         : nullptr);
      data.push_back(entry_present[2]
                         ? std::make_unique<PasswordFormData>(
                               android_correct2(dates[date_index++]))
                         : nullptr);
      data.push_back(entry_present[3]
                         ? std::make_unique<PasswordFormData>(
                               android_incorrect2(dates[date_index++]))
                         : nullptr);

      SCOPED_TRACE(val);
      SCOPED_TRACE(j);
      SCOPED_TRACE(FormDataMessage("data[0]=", data[0].get()));
      SCOPED_TRACE(FormDataMessage("data[1]=", data[1].get()));
      SCOPED_TRACE(FormDataMessage("data[2]=", data[2].get()));
      SCOPED_TRACE(FormDataMessage("data[3]=", data[3].get()));

      const PasswordFormData* latest_data =
          std::find_if(data.begin(), data.end(),
                       [latest](const std::unique_ptr<PasswordFormData>& data) {
                         return data && data->creation_time == latest;
                       })
              ->get();
      ASSERT_TRUE(latest_data);
      std::vector<std::pair<SyncChange::SyncChangeType, autofill::PasswordForm>>
          expected_sync_updates;
      for (int i = 0; i < 2; ++i) {
        if (latest_data != data[i].get()) {
          expected_sync_updates.push_back(std::make_pair(
              data[i] ? SyncChange::ACTION_UPDATE : SyncChange::ACTION_ADD,
              i == 0 ? FormWithCorrectTag(*latest_data)
                     : FormWithAndroidAutofillTag(*latest_data)));
        }
      }

      std::vector<autofill::PasswordForm> stored_forms;
      for (int i = 2; i < 4; ++i) {
        if (data[i])
          stored_forms.push_back(*FillPasswordFormWithData(*data[i]));
      }

      SyncDataList sync_list;
      for (int i = 0; i < 2; ++i) {
        if (data[i]) {
          sync_list.push_back(
              SyncDataFromPassword(*FillPasswordFormWithData(*data[i])));
        }
      }

      for (bool swap_sync_list : {false, true}) {
        auto wrapper = std::make_unique<PasswordSyncableServiceWrapper>();
        auto processor =
            std::make_unique<testing::StrictMock<MockSyncChangeProcessor>>();

        EXPECT_CALL(*wrapper->password_store(), FillAutofillableLogins(_))
            .WillOnce(AppendForms(stored_forms));
        EXPECT_CALL(*wrapper->password_store(), FillBlacklistLogins(_))
            .WillOnce(Return(true));
        for (int i = 2; i < 4; ++i) {
          if (latest_data != data[i].get()) {
            autofill::PasswordForm latest_form =
                i == 2 ? FormWithCorrectTag(*latest_data)
                       : FormWithAndroidAutofillTag(*latest_data);
            if (data[i]) {
              EXPECT_CALL(*wrapper->password_store(),
                          UpdateLoginImpl(latest_form));
            } else {
              EXPECT_CALL(*wrapper->password_store(),
                          AddLoginImpl(latest_form));
            }
          }
        }

        if (expected_sync_updates.size() == 0) {
          EXPECT_CALL(*processor, ProcessSyncChanges(_, IsEmpty()));
        } else if (expected_sync_updates.size() == 1) {
          EXPECT_CALL(
              *processor,
              ProcessSyncChanges(_, ElementsAre(SyncChangeIs(
                                        expected_sync_updates[0].first,
                                        expected_sync_updates[0].second))));
        } else if (expected_sync_updates.size() == 2) {
          EXPECT_CALL(
              *processor,
              ProcessSyncChanges(
                  _, UnorderedElementsAre(
                         SyncChangeIs(expected_sync_updates[0].first,
                                      expected_sync_updates[0].second),
                         SyncChangeIs(expected_sync_updates[1].first,
                                      expected_sync_updates[1].second))));
        }

        if (swap_sync_list && sync_list.size() == 2)
          std::swap(sync_list[0], sync_list[1]);
        wrapper->service()->MergeDataAndStartSyncing(
            syncer::PASSWORDS, sync_list, std::move(processor),
            std::unique_ptr<syncer::SyncErrorFactory>());
        wrapper.reset();
        // Wait til PasswordStore is destroy end therefore all the expectations
        // on it are checked.
        scoped_task_environment_.RunUntilIdle();
      }
    }
  }
}

TEST_F(PasswordSyncableServiceAndroidAutofillTest, TwoWayServerAndLocalMerge) {
  for (unsigned i = 0; i < 2 * 2; ++i) {
    // Generate 4 different combinations for local/server entries.
    std::vector<unsigned> combination = ExtractTimestamps(i, 2);
    ASSERT_EQ(2u, combination.size());
    const bool sync_data_correct = !!combination[0];
    const bool local_data_correct = !!combination[1];

    for (unsigned val = 0; val < 2 * 2; ++val) {
      std::vector<unsigned> dates = ExtractTimestamps(i, 2);
      ASSERT_EQ(2u, dates.size());

      const PasswordFormData sync_data = sync_data_correct
                                             ? android_correct(dates[0])
                                             : android_incorrect(dates[0]);
      const PasswordFormData local_data = local_data_correct
                                              ? android_correct2(dates[1])
                                              : android_incorrect2(dates[1]);

      const PasswordFormData* latest_data =
          dates[1] > dates[0] ? &local_data : &sync_data;

      auto wrapper = std::make_unique<PasswordSyncableServiceWrapper>();
      auto processor =
          std::make_unique<testing::StrictMock<MockSyncChangeProcessor>>();

      EXPECT_CALL(*wrapper->password_store(), FillAutofillableLogins(_))
          .WillOnce(AppendForm(*FillPasswordFormWithData(local_data)));
      EXPECT_CALL(*wrapper->password_store(), FillBlacklistLogins(_))
          .WillOnce(Return(true));
      if (!local_data_correct || latest_data == &sync_data) {
        if (local_data_correct) {
          EXPECT_CALL(*wrapper->password_store(),
                      UpdateLoginImpl(FormWithCorrectTag(*latest_data)));
        } else {
          EXPECT_CALL(*wrapper->password_store(),
                      AddLoginImpl(FormWithCorrectTag(*latest_data)));
        }
      }
      if (!local_data_correct && latest_data == &sync_data) {
        EXPECT_CALL(*wrapper->password_store(),
                    UpdateLoginImpl(FormWithAndroidAutofillTag(*latest_data)));
      } else if (local_data_correct && !sync_data_correct) {
        EXPECT_CALL(*wrapper->password_store(),
                    AddLoginImpl(FormWithAndroidAutofillTag(*latest_data)));
      }

      std::vector<std::pair<SyncChange::SyncChangeType, autofill::PasswordForm>>
          expected_sync_updates;
      // Deal with the correct sync entry and incorrect one.
      if (sync_data_correct) {
        if (latest_data != &sync_data) {
          expected_sync_updates.push_back(std::make_pair(
              SyncChange::ACTION_UPDATE, FormWithCorrectTag(*latest_data)));
        }
        if (!local_data_correct) {
          expected_sync_updates.push_back(
              std::make_pair(SyncChange::ACTION_ADD,
                             FormWithAndroidAutofillTag(*latest_data)));
        }
      } else {
        expected_sync_updates.push_back(std::make_pair(
            SyncChange::ACTION_ADD, FormWithCorrectTag(*latest_data)));
        if (latest_data != &sync_data) {
          expected_sync_updates.push_back(
              std::make_pair(SyncChange::ACTION_UPDATE,
                             FormWithAndroidAutofillTag(*latest_data)));
        }
      }

      // Set expectation on |processor|.
      if (expected_sync_updates.size() == 0) {
        EXPECT_CALL(*processor, ProcessSyncChanges(_, IsEmpty()));
      } else if (expected_sync_updates.size() == 1) {
        EXPECT_CALL(
            *processor,
            ProcessSyncChanges(
                _, ElementsAre(SyncChangeIs(expected_sync_updates[0].first,
                                            expected_sync_updates[0].second))));
      } else if (expected_sync_updates.size() == 2) {
        EXPECT_CALL(*processor,
                    ProcessSyncChanges(
                        _, UnorderedElementsAre(
                               SyncChangeIs(expected_sync_updates[0].first,
                                            expected_sync_updates[0].second),
                               SyncChangeIs(expected_sync_updates[1].first,
                                            expected_sync_updates[1].second))));
      }

      SyncDataList sync_list = {
          SyncDataFromPassword(*FillPasswordFormWithData(sync_data))};
      wrapper->service()->MergeDataAndStartSyncing(
          syncer::PASSWORDS, sync_list, std::move(processor),
          std::unique_ptr<syncer::SyncErrorFactory>());
      wrapper.reset();
      // Wait til PasswordStore is destroy end therefore all the expectations on
      // it are checked.
      scoped_task_environment_.RunUntilIdle();
    }
  }
}

TEST_F(PasswordSyncableServiceAndroidAutofillTest, OneEntryOnly) {
  for (int i = 0; i < 3; ++i) {
    // The case when only local incorrect entry exists is excluded. It's very
    // exotic because a local incorrect entry can come only from the server.
    // In such a case a copy will be uploaded to the server and next
    // MergeDataAndStartSyncing will do a proper migration.
    SCOPED_TRACE(i);
    // Whether the entry exists: Sync correct, Sync Android autofill,
    // local correct, local incorrect.
    const bool entry_is_correct = i == 0 || i == 2;
    const bool entry_is_local = i >= 2;
    PasswordFormData data =
        entry_is_correct ? android_correct(100) : android_incorrect(100);

    auto wrapper = std::make_unique<PasswordSyncableServiceWrapper>();
    auto processor =
        std::make_unique<testing::StrictMock<MockSyncChangeProcessor>>();

    if (entry_is_local) {
      EXPECT_CALL(*wrapper->password_store(), FillAutofillableLogins(_))
          .WillOnce(AppendForm(*FillPasswordFormWithData(data)));
    } else {
      EXPECT_CALL(*wrapper->password_store(), FillAutofillableLogins(_))
          .WillOnce(Return(true));
    }
    EXPECT_CALL(*wrapper->password_store(), FillBlacklistLogins(_))
        .WillOnce(Return(true));
    if (!entry_is_local && !entry_is_correct) {
      EXPECT_CALL(*wrapper->password_store(),
                  AddLoginImpl(FormWithAndroidAutofillTag(data)));
    }
    if (!entry_is_local) {
      EXPECT_CALL(*wrapper->password_store(),
                  AddLoginImpl(FormWithCorrectTag(data)));
    }

    if (entry_is_correct && !entry_is_local) {
      EXPECT_CALL(*processor, ProcessSyncChanges(_, IsEmpty()));
    } else {
      EXPECT_CALL(*processor,
                  ProcessSyncChanges(
                      _, ElementsAre(SyncChangeIs(SyncChange::ACTION_ADD,
                                                  FormWithCorrectTag(data)))));
    }

    SyncDataList sync_list;
    if (!entry_is_local) {
      sync_list.push_back(
          SyncDataFromPassword(*FillPasswordFormWithData(data)));
    }
    wrapper->service()->MergeDataAndStartSyncing(
        syncer::PASSWORDS, sync_list, std::move(processor),
        std::unique_ptr<syncer::SyncErrorFactory>());
    wrapper.reset();
    // Wait til PasswordStore is destroy end therefore all the expectations on
    // it are checked.
    scoped_task_environment_.RunUntilIdle();
  }
}

TEST_F(PasswordSyncableServiceAndroidAutofillTest, FourEqualEntries) {
  // Sync correct, Sync Android autofill, local correct, local incorrect with
  // the same content. Nothing should happen.
  const PasswordFormData data = android_correct(100);
  autofill::PasswordForm local_correct = FormWithCorrectTag(data);
  autofill::PasswordForm local_incorrect = FormWithAndroidAutofillTag(data);
  syncer::SyncData sync_correct = SyncDataFromPassword(local_correct);
  syncer::SyncData sync_incorrect = SyncDataFromPassword(local_incorrect);

  for (bool correct_sync_first : {true, false}) {
    auto wrapper = std::make_unique<PasswordSyncableServiceWrapper>();
    auto processor =
        std::make_unique<testing::StrictMock<MockSyncChangeProcessor>>();

    std::vector<autofill::PasswordForm> stored_forms = {local_correct,
                                                        local_incorrect};
    EXPECT_CALL(*wrapper->password_store(), FillAutofillableLogins(_))
        .WillOnce(AppendForms(stored_forms));
    EXPECT_CALL(*wrapper->password_store(), FillBlacklistLogins(_))
        .WillOnce(Return(true));
    EXPECT_CALL(*processor, ProcessSyncChanges(_, IsEmpty()));

    SyncDataList sync_list = {sync_correct, sync_incorrect};
    if (!correct_sync_first)
      std::swap(sync_list[0], sync_list[1]);
    wrapper->service()->MergeDataAndStartSyncing(
        syncer::PASSWORDS, sync_list, std::move(processor),
        std::unique_ptr<syncer::SyncErrorFactory>());
    wrapper.reset();
    // Wait til PasswordStore is destroy end therefore all the expectations on
    // it are checked.
    scoped_task_environment_.RunUntilIdle();
  }
}

TEST_F(PasswordSyncableServiceAndroidAutofillTest, AndroidCorrectEqualEntries) {
  // Sync correct, local correct with the same content. Nothing should happen.
  const PasswordFormData data = android_correct(100);
  autofill::PasswordForm local_correct = FormWithCorrectTag(data);
  syncer::SyncData sync_correct = SyncDataFromPassword(local_correct);

  auto wrapper = std::make_unique<PasswordSyncableServiceWrapper>();
  auto processor =
      std::make_unique<testing::StrictMock<MockSyncChangeProcessor>>();

  EXPECT_CALL(*wrapper->password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(local_correct));
  EXPECT_CALL(*wrapper->password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*processor, ProcessSyncChanges(_, IsEmpty()));

  wrapper->service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS, {sync_correct}, std::move(processor),
      std::unique_ptr<syncer::SyncErrorFactory>());
  wrapper.reset();
  // Wait til PasswordStore is destroy end therefore all the expectations on
  // it are checked.
  scoped_task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace password_manager

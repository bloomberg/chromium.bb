// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_syncable_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::SyncChange;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using testing::AnyNumber;
using testing::ElementsAre;
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
const autofill::PasswordForm::Type kArbitraryType =
    autofill::PasswordForm::TYPE_GENERATED;
const char kAvatarUrl[] = "https://fb.com/Avatar";
const char kDisplayName[] = "Agent Smith";
const char kFederationUrl[] = "https://fb.com/federation_url";
const char kSignonRealm[] = "abc";
const char kSignonRealm2[] = "def";
const char kSignonRealm3[] = "xyz";
const int kTimesUsed = 5;

typedef std::vector<SyncChange> SyncChangeList;

const sync_pb::PasswordSpecificsData& GetPasswordSpecifics(
    const syncer::SyncData& sync_data) {
  return sync_data.GetSpecifics().password().client_only_encrypted_data();
}

MATCHER(HasDateSynced, "") {
  return !arg.date_synced.is_null() && !arg.date_synced.is_max();
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
      expected_password.ssl_valid() == actual_password.ssl_valid() &&
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
  autofill::PasswordForm form = PasswordFromSpecifics(
      GetPasswordSpecifics(data));
  return (arg.change_type() == change_type &&
          syncer::SyncDataLocal(data).GetTag() ==
              MakePasswordSyncTag(password) &&
          (change_type == SyncChange::ACTION_DELETE ||
           Matches(PasswordIs(password))(form)));
}

// The argument is std::vector<autofill::PasswordForm*>*. The caller is
// responsible for the lifetime of all the password forms.
ACTION_P(AppendForm, form) {
  arg0->push_back(new autofill::PasswordForm(form));
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
  password_specifics->set_avatar_url("https://accounts.google.com/Avatar");
  password_specifics->set_federation_url("https://google.com/federation");

  std::string tag = MakePasswordSyncTag(*password_specifics);
  return syncer::SyncData::CreateLocalData(tag, tag, password_data);
}

SyncChange CreateSyncChange(const autofill::PasswordForm& password,
                            SyncChange::SyncChangeType type) {
  SyncData data = SyncDataFromPassword(password);
  return SyncChange(FROM_HERE, type, data);
}

// A testable implementation of the |PasswordSyncableService| that mocks
// out all interaction with the password database.
class MockPasswordSyncableService : public PasswordSyncableService {
 public:
  explicit MockPasswordSyncableService(PasswordStoreSync* password_store)
      : PasswordSyncableService(password_store) {}

  MOCK_METHOD1(StartSyncFlare, void(syncer::ModelType));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordSyncableService);
};

// Mock implementation of SyncChangeProcessor.
class MockSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() {}

  MOCK_METHOD2(ProcessSyncChanges,
               SyncError(const tracked_objects::Location&,
                         const SyncChangeList& list));
  virtual SyncDataList GetAllSyncData(syncer::ModelType type) const OVERRIDE {
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
    service_.reset(new MockPasswordSyncableService(
        password_store_->GetSyncInterface()));
    ON_CALL(*password_store_, AddLoginImpl(HasDateSynced()))
        .WillByDefault(Return(PasswordStoreChangeList()));
    ON_CALL(*password_store_, RemoveLoginImpl(_))
        .WillByDefault(Return(PasswordStoreChangeList()));
    ON_CALL(*password_store_, UpdateLoginImpl(HasDateSynced()))
        .WillByDefault(Return(PasswordStoreChangeList()));
    EXPECT_CALL(*password_store(), NotifyLoginsChanged(_)).Times(AnyNumber());
  }

  ~PasswordSyncableServiceWrapper() {
    password_store_->Shutdown();
  }

  MockPasswordStore* password_store() { return password_store_.get(); }

  MockPasswordSyncableService* service() { return service_.get(); }

  // Returnes the scoped_ptr to |service_| thus NULLing out it.
  scoped_ptr<syncer::SyncChangeProcessor> ReleaseSyncableService() {
    return service_.PassAs<syncer::SyncChangeProcessor>();
  }

 private:
  scoped_refptr<MockPasswordStore> password_store_;
  scoped_ptr<MockPasswordSyncableService> service_;

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
  MockPasswordSyncableService* service() { return wrapper_.service(); }

 protected:
  scoped_ptr<MockSyncChangeProcessor> processor_;

 private:
  PasswordSyncableServiceWrapper wrapper_;
};


// Both sync and password db have data that are not present in the other.
TEST_F(PasswordSyncableServiceTest, AdditionsInBoth) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;

  SyncDataList list;
  list.push_back(CreateSyncData(kSignonRealm2));
  autofill::PasswordForm new_from_sync = PasswordFromSpecifics(
      GetPasswordSpecifics(list.back()));

  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(new_from_sync)));
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, ElementsAre(
      SyncChangeIs(SyncChange::ACTION_ADD, form))));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS,
      list,
      processor_.PassAs<syncer::SyncChangeProcessor>(),
      scoped_ptr<syncer::SyncErrorFactory>());
}

// Sync has data that is not present in the password db.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInSync) {
  SyncDataList list;
  list.push_back(CreateSyncData(kSignonRealm));
  autofill::PasswordForm new_from_sync = PasswordFromSpecifics(
      GetPasswordSpecifics(list.back()));

  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(new_from_sync)));
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS,
      list,
      processor_.PassAs<syncer::SyncChangeProcessor>(),
      scoped_ptr<syncer::SyncErrorFactory>());
}

// Passwords db has data that is not present in sync.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInPasswordStore) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.times_used = kTimesUsed;
  form.type = kArbitraryType;
  form.display_name = base::ASCIIToUTF16(kDisplayName);
  form.avatar_url = GURL(kAvatarUrl);
  form.federation_url = GURL(kFederationUrl);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));

  EXPECT_CALL(*processor_, ProcessSyncChanges(_, ElementsAre(
      SyncChangeIs(SyncChange::ACTION_ADD, form))));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS,
      SyncDataList(),
      processor_.PassAs<syncer::SyncChangeProcessor>(),
      scoped_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync contain the same data.
TEST_F(PasswordSyncableServiceTest, BothInSync) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  form.times_used = kTimesUsed;
  form.type = kArbitraryType;
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));

  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS,
      SyncDataList(1, SyncDataFromPassword(form)),
      processor_.PassAs<syncer::SyncChangeProcessor>(),
      scoped_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync have the same data but they need to be merged
// as some fields of the data differ.
TEST_F(PasswordSyncableServiceTest, Merge) {
  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  form1.action = GURL("http://pie.com");
  form1.date_created = base::Time::Now();
  form1.preferred = true;

  autofill::PasswordForm form2(form1);
  form2.preferred = false;
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form1));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_store(), UpdateLoginImpl(PasswordIs(form2)));
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, IsEmpty()));

  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS,
      SyncDataList(1, SyncDataFromPassword(form2)),
      processor_.PassAs<syncer::SyncChangeProcessor>(),
      scoped_ptr<syncer::SyncErrorFactory>());
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
      syncer::PASSWORDS,
      SyncDataList(),
      processor_.PassAs<syncer::SyncChangeProcessor>(),
      scoped_ptr<syncer::SyncErrorFactory>());

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
  autofill::PasswordForm deleted_form;
  deleted_form.signon_realm = kSignonRealm2;
  deleted_form.action = GURL("http://bar.com");
  deleted_form.blacklisted_by_user = true;

  SyncData add_data = CreateSyncData(kSignonRealm3);
  autofill::PasswordForm new_from_sync = PasswordFromSpecifics(
      GetPasswordSpecifics(add_data));

  SyncChangeList list;
  list.push_back(SyncChange(FROM_HERE,
                            syncer::SyncChange::ACTION_ADD,
                            add_data));
  list.push_back(CreateSyncChange(updated_form,
                                  syncer::SyncChange::ACTION_UPDATE));
  list.push_back(CreateSyncChange(deleted_form,
                                  syncer::SyncChange::ACTION_DELETE));
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
  form1.avatar_url = GURL(kAvatarUrl);
  form1.federation_url = GURL(kFederationUrl);
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
  for (SyncDataList::iterator it = actual_list.begin();
       it != actual_list.end(); ++it) {
    actual_form_list.push_back(
        PasswordFromSpecifics(GetPasswordSpecifics(*it)));
  }
  EXPECT_THAT(actual_form_list, UnorderedElementsAre(PasswordIs(form1),
                                                     PasswordIs(form2)));
}

// Creates 2 PasswordSyncableService instances, merges the content of the first
// one to the second one and back.
TEST_F(PasswordSyncableServiceTest, MergeDataAndPushBack) {
  autofill::PasswordForm form1;
  form1.signon_realm = kSignonRealm;
  form1.action = GURL("http://foo.com");

  PasswordSyncableServiceWrapper other_service_wrapper;
  autofill::PasswordForm form2;
  form2.signon_realm = kSignonRealm2;
  form2.action = GURL("http://bar.com");
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(AppendForm(form1));
  EXPECT_CALL(*password_store(), FillBlacklistLogins(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*other_service_wrapper.password_store(),
              FillAutofillableLogins(_)).WillOnce(AppendForm(form2));
  EXPECT_CALL(*other_service_wrapper.password_store(),
              FillBlacklistLogins(_)).WillOnce(Return(true));

  EXPECT_CALL(*password_store(), AddLoginImpl(PasswordIs(form2)));
  EXPECT_CALL(*other_service_wrapper.password_store(),
              AddLoginImpl(PasswordIs(form1)));

  syncer::SyncDataList other_service_data =
      other_service_wrapper.service()->GetAllSyncData(syncer::PASSWORDS);
  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS,
      other_service_data,
      other_service_wrapper.ReleaseSyncableService(),
      scoped_ptr<syncer::SyncErrorFactory>());
}

// Calls ActOnPasswordStoreChanges without SyncChangeProcessor. StartSyncFlare
// should be called.
TEST_F(PasswordSyncableServiceTest, StartSyncFlare) {
  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));

  // No flare and no SyncChangeProcessor, the call shouldn't crash.
  service()->ActOnPasswordStoreChanges(list);

  // Set the flare. It should be called as there is no SyncChangeProcessor.
  service()->InjectStartSyncFlare(
      base::Bind(&MockPasswordSyncableService::StartSyncFlare,
                 base::Unretained(service())));
  EXPECT_CALL(*service(), StartSyncFlare(syncer::PASSWORDS));
  service()->ActOnPasswordStoreChanges(list);
}

// Start syncing with an error. Subsequent password store updates shouldn't be
// propagated to Sync.
TEST_F(PasswordSyncableServiceTest, FailedReadFromPasswordStore) {
  scoped_ptr<syncer::SyncErrorFactoryMock> error_factory(
      new syncer::SyncErrorFactoryMock);
  EXPECT_CALL(*password_store(), FillAutofillableLogins(_))
      .WillOnce(Return(false));
  EXPECT_CALL(*error_factory, CreateAndUploadError(_, _))
      .WillOnce(Return(SyncError()));
  // ActOnPasswordStoreChanges() below shouldn't generate any changes for Sync.
  // |processor_| will be destroyed in MergeDataAndStartSyncing().
  EXPECT_CALL(*processor_, ProcessSyncChanges(_, _)).Times(0);
  service()->MergeDataAndStartSyncing(
      syncer::PASSWORDS,
      syncer::SyncDataList(),
      processor_.PassAs<syncer::SyncChangeProcessor>(),
      error_factory.PassAs<syncer::SyncErrorFactory>());

  autofill::PasswordForm form;
  form.signon_realm = kSignonRealm;
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  service()->ActOnPasswordStoreChanges(list);
}

}  // namespace

}  // namespace password_manager

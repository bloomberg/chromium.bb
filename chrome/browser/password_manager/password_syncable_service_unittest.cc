// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_syncable_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/password_manager/mock_password_store.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::SyncChange;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::_;

namespace {

typedef std::vector<SyncChange> SyncChangeList;

const sync_pb::PasswordSpecificsData& GetPasswordSpecifics(
    const syncer::SyncData& sync_change) {
  const sync_pb::EntitySpecifics& specifics = sync_change.GetSpecifics();
  return specifics.password().client_only_encrypted_data();
}

void PasswordsEqual(const sync_pb::PasswordSpecificsData& expected_password,
                    const sync_pb::PasswordSpecificsData& actual_password) {
  EXPECT_EQ(expected_password.scheme(), actual_password.scheme());
  EXPECT_EQ(expected_password.signon_realm(), actual_password.signon_realm());
  EXPECT_EQ(expected_password.origin(), actual_password.origin());
  EXPECT_EQ(expected_password.action(), actual_password.action());
  EXPECT_EQ(expected_password.username_element(),
            actual_password.username_element());
  EXPECT_EQ(expected_password.password_element(),
            actual_password.password_element());
  EXPECT_EQ(expected_password.username_value(),
            actual_password.username_value());
  EXPECT_EQ(expected_password.password_value(),
            actual_password.password_value());
  EXPECT_EQ(expected_password.ssl_valid(), actual_password.ssl_valid());
  EXPECT_EQ(expected_password.preferred(), actual_password.preferred());
  EXPECT_EQ(expected_password.date_created(), actual_password.date_created());
  EXPECT_EQ(expected_password.blacklisted(), actual_password.blacklisted());
}

// Creates a sync data consisting of password specifics. The sign on realm is
// set to |signon_realm|.
SyncData CreateSyncData(const std::string& signon_realm) {
  sync_pb::EntitySpecifics password_data;
  sync_pb::PasswordSpecificsData* password_specifics =
      password_data.mutable_password()->mutable_client_only_encrypted_data();
  password_specifics->set_signon_realm(signon_realm);

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
  explicit MockPasswordSyncableService(PasswordStore* password_store)
      : PasswordSyncableService(password_store) {}
  virtual ~MockPasswordSyncableService() {}

  MOCK_METHOD1(NotifyPasswordStoreOfLoginChanges,
               void (const PasswordStoreChangeList&));
};

// Class to verify the arguments passed to |PasswordStore|.
class PasswordStoreDataVerifier {
 public:
  PasswordStoreDataVerifier() {}
  ~PasswordStoreDataVerifier() {
    EXPECT_TRUE(expected_db_add_changes_.empty());
    EXPECT_TRUE(expected_db_update_changes_.empty());
  }

  class TestSyncChangeProcessor;

  // Sets expected changes to the password database.
  void SetExpectedDBChanges(
      const SyncDataList& add_forms,
      const std::vector<autofill::PasswordForm*>& update_forms,
      MockPasswordStore* password_store);
  // Sets expected changes to TestSyncChangeProcessor.
  void SetExpectedSyncChanges(SyncChangeList list);

 private:
  // Checks that |change_list| matches |expected_sync_change_list_|.
  SyncError TestSyncChanges(const SyncChangeList& change_list);

  // Verifies that the |password| is present in the |expected_db_add_changes_|
  // list. If found |password| would be removed from
  // |expected_db_add_changes_| list.
  PasswordStoreChangeList VerifyAdd(const autofill::PasswordForm& password) {
    return VerifyChange(PasswordStoreChange::ADD, password,
                        &expected_db_add_changes_);
  }

  // Verifies that the |password| is present in the
  // |expected_db_update_changes_| list.
  PasswordStoreChangeList VerifyUpdate(const autofill::PasswordForm& password) {
    return VerifyChange(PasswordStoreChange::UPDATE, password,
                        &expected_db_update_changes_);
  }

  static PasswordStoreChangeList VerifyChange(
      PasswordStoreChange::Type type,
      const autofill::PasswordForm& password,
      std::vector<autofill::PasswordForm>* password_list);

  std::vector<autofill::PasswordForm> expected_db_add_changes_;
  std::vector<autofill::PasswordForm> expected_db_update_changes_;
  SyncChangeList expected_sync_change_list_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDataVerifier);
};

class PasswordStoreDataVerifier::TestSyncChangeProcessor
    : public syncer::SyncChangeProcessor {
 public:
  explicit TestSyncChangeProcessor(PasswordStoreDataVerifier* verifier)
      : verifier_(verifier) {
  }
  virtual ~TestSyncChangeProcessor() {}

  virtual SyncError ProcessSyncChanges(const tracked_objects::Location&,
                                       const SyncChangeList& list) OVERRIDE {
    return verifier_->TestSyncChanges(list);
  }

  virtual SyncDataList GetAllSyncData(syncer::ModelType type) const OVERRIDE {
    return SyncDataList();
  }
 private:
  PasswordStoreDataVerifier* verifier_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncChangeProcessor);
};

void PasswordStoreDataVerifier::SetExpectedDBChanges(
    const SyncDataList& add_forms,
    const std::vector<autofill::PasswordForm*>& update_forms,
    MockPasswordStore* password_store) {
  DCHECK(expected_db_add_changes_.empty());
  DCHECK(expected_db_update_changes_.empty());
  DCHECK(password_store);

  for (SyncDataList::const_iterator i = add_forms.begin();
       i != add_forms.end(); ++i) {
    autofill::PasswordForm form;
    PasswordFromSpecifics(GetPasswordSpecifics(*i), &form);
    expected_db_add_changes_.push_back(form);
  }
  if (expected_db_add_changes_.empty()) {
    EXPECT_CALL(*password_store, AddLoginImpl(_)).Times(0);
  } else {
    EXPECT_CALL(*password_store, AddLoginImpl(_))
        .Times(expected_db_add_changes_.size())
        .WillRepeatedly(Invoke(this, &PasswordStoreDataVerifier::VerifyAdd));
  }

  for (std::vector<autofill::PasswordForm*>::const_iterator i =
           update_forms.begin();
       i != update_forms.end(); ++i) {
    expected_db_update_changes_.push_back(**i);
  }
  if (expected_db_update_changes_.empty()) {
    EXPECT_CALL(*password_store, UpdateLoginImpl(_)).Times(0);
  } else {
    EXPECT_CALL(*password_store, UpdateLoginImpl(_))
        .Times(expected_db_update_changes_.size())
        .WillRepeatedly(Invoke(this, &PasswordStoreDataVerifier::VerifyUpdate));
  }
}

void PasswordStoreDataVerifier::SetExpectedSyncChanges(SyncChangeList list) {
  expected_sync_change_list_.swap(list);
}

SyncError PasswordStoreDataVerifier::TestSyncChanges(
    const SyncChangeList& change_list) {
  for (SyncChangeList::const_iterator it = change_list.begin();
      it != change_list.end(); ++it) {
    const SyncChange& data = *it;
    const sync_pb::PasswordSpecificsData& actual_password(
        GetPasswordSpecifics(data.sync_data()));
    std::string actual_tag = MakePasswordSyncTag(actual_password);

    bool matched = false;
    for (SyncChangeList::iterator expected_it =
             expected_sync_change_list_.begin();
         expected_it != expected_sync_change_list_.end();
         ++expected_it) {
      const sync_pb::PasswordSpecificsData& expected_password(
          GetPasswordSpecifics(expected_it->sync_data()));
      if (actual_tag == MakePasswordSyncTag(expected_password)) {
        PasswordsEqual(expected_password, actual_password);
        EXPECT_EQ(expected_it->change_type(), data.change_type());
        matched = true;
        break;
      }
    }
    EXPECT_TRUE(matched) << actual_tag;
  }
  EXPECT_EQ(expected_sync_change_list_.size(), change_list.size());
  return SyncError();
}

// static
PasswordStoreChangeList PasswordStoreDataVerifier::VerifyChange(
    PasswordStoreChange::Type type,
    const autofill::PasswordForm& password,
    std::vector<autofill::PasswordForm>* password_list) {
  std::vector<autofill::PasswordForm>::iterator it =
      std::find(password_list->begin(), password_list->end(), password);
  EXPECT_NE(password_list->end(), it);
  password_list->erase(it);
  return PasswordStoreChangeList(1, PasswordStoreChange(type, password));
}

class PasswordSyncableServiceTest : public testing::Test {
 public:
  PasswordSyncableServiceTest() {}
  virtual ~PasswordSyncableServiceTest() {}

  virtual void SetUp() OVERRIDE {
    password_store_ = new MockPasswordStore;
    service_.reset(new MockPasswordSyncableService(password_store_));
  }

  virtual void TearDown() OVERRIDE {
    password_store_->Shutdown();
  }

  PasswordStoreDataVerifier* verifier() {
    return &verifier_;
  }

  scoped_ptr<syncer::SyncChangeProcessor> ReleaseSyncChangeProcessor() {
    return make_scoped_ptr<syncer::SyncChangeProcessor>(
        new PasswordStoreDataVerifier::TestSyncChangeProcessor(verifier()));
  }

  // Sets the data that will be returned to the caller accessing password store.
  void SetPasswordStoreData(const std::vector<autofill::PasswordForm*>& forms) {
    EXPECT_CALL(*password_store_, FillAutofillableLogins(_))
        .WillOnce(DoAll(SetArgPointee<0>(forms), Return(true)));
  }

 protected:
  scoped_refptr<MockPasswordStore> password_store_;
  scoped_ptr<MockPasswordSyncableService> service_;
  PasswordStoreDataVerifier verifier_;
};


// Both sync and password db have data that are not present in the other.
TEST_F(PasswordSyncableServiceTest, AdditionsInBoth) {
  autofill::PasswordForm* form1 = new autofill::PasswordForm;
  form1->signon_realm = "abc";
  std::vector<autofill::PasswordForm*> forms;
  forms.push_back(form1);
  SetPasswordStoreData(forms);

  SyncData sync_data = CreateSyncData("def");
  SyncDataList list;
  list.push_back(sync_data);

  verifier()->SetExpectedDBChanges(list,
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store_);
  verifier()->SetExpectedSyncChanges(
      SyncChangeList(1, CreateSyncChange(*form1, SyncChange::ACTION_ADD)));
  EXPECT_CALL(*service_, NotifyPasswordStoreOfLoginChanges(_));

  service_->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                     list,
                                     ReleaseSyncChangeProcessor(),
                                     scoped_ptr<syncer::SyncErrorFactory>());
}

// Sync has data that is not present in the password db.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInSync) {
  SetPasswordStoreData(std::vector<autofill::PasswordForm*>());

  SyncData sync_data = CreateSyncData("def");
  SyncDataList list;
  list.push_back(sync_data);

  verifier()->SetExpectedDBChanges(list,
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store_);
  verifier()->SetExpectedSyncChanges(SyncChangeList());
  EXPECT_CALL(*service_, NotifyPasswordStoreOfLoginChanges(_));

  service_->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                     list,
                                     ReleaseSyncChangeProcessor(),
                                     scoped_ptr<syncer::SyncErrorFactory>());
}

// Passwords db has data that is not present in sync.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInPasswordStore) {
  autofill::PasswordForm* form1 = new autofill::PasswordForm;
  form1->signon_realm = "abc";
  std::vector<autofill::PasswordForm*> forms;
  forms.push_back(form1);
  SetPasswordStoreData(forms);

  verifier()->SetExpectedDBChanges(SyncDataList(),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store_);
  verifier()->SetExpectedSyncChanges(
      SyncChangeList(1, CreateSyncChange(*form1, SyncChange::ACTION_ADD)));
  EXPECT_CALL(*service_,
              NotifyPasswordStoreOfLoginChanges(PasswordStoreChangeList()));

  service_->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                     SyncDataList(),
                                     ReleaseSyncChangeProcessor(),
                                     scoped_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync contain the same data.
TEST_F(PasswordSyncableServiceTest, BothInSync) {
  autofill::PasswordForm *form1 = new autofill::PasswordForm;
  form1->signon_realm = "abc";
  std::vector<autofill::PasswordForm*> forms;
  forms.push_back(form1);
  SetPasswordStoreData(forms);

  verifier()->SetExpectedDBChanges(SyncDataList(),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store_);
  verifier()->SetExpectedSyncChanges(SyncChangeList());
  EXPECT_CALL(*service_,
              NotifyPasswordStoreOfLoginChanges(PasswordStoreChangeList()));

  service_->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                     SyncDataList(1, CreateSyncData("abc")),
                                     ReleaseSyncChangeProcessor(),
                                     scoped_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync have the same data but they need to be merged
// as some fields of the data differ.
TEST_F(PasswordSyncableServiceTest, Merge) {
  autofill::PasswordForm *form1 = new autofill::PasswordForm;
  form1->signon_realm = "abc";
  form1->action = GURL("http://pie.com");
  form1->date_created = base::Time::Now();
  std::vector<autofill::PasswordForm*> forms;
  forms.push_back(form1);
  SetPasswordStoreData(forms);

  verifier()->SetExpectedDBChanges(SyncDataList(),
                                   forms,
                                   password_store_);
  verifier()->SetExpectedSyncChanges(
      SyncChangeList(1, CreateSyncChange(*form1, SyncChange::ACTION_UPDATE)));

  EXPECT_CALL(*service_, NotifyPasswordStoreOfLoginChanges(_));

  service_->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                     SyncDataList(1, CreateSyncData("abc")),
                                     ReleaseSyncChangeProcessor(),
                                     scoped_ptr<syncer::SyncErrorFactory>());
}

// Initiate sync due to local DB changes.
TEST_F(PasswordSyncableServiceTest, PasswordStoreChanges) {
  // Set the sync change processor first.
  SetPasswordStoreData(std::vector<autofill::PasswordForm*>());
  verifier()->SetExpectedSyncChanges(SyncChangeList());
  EXPECT_CALL(*service_,
              NotifyPasswordStoreOfLoginChanges(PasswordStoreChangeList()));
  service_->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                     SyncDataList(),
                                     ReleaseSyncChangeProcessor(),
                                     scoped_ptr<syncer::SyncErrorFactory>());

  autofill::PasswordForm form1;
  form1.signon_realm = "abc";
  autofill::PasswordForm form2;
  form2.signon_realm = "def";
  autofill::PasswordForm form3;
  form3.signon_realm = "xyz";

  SyncChangeList sync_list;
  sync_list.push_back(CreateSyncChange(form1, SyncChange::ACTION_ADD));
  sync_list.push_back(CreateSyncChange(form2, SyncChange::ACTION_UPDATE));
  sync_list.push_back(CreateSyncChange(form3, SyncChange::ACTION_DELETE));

  verifier()->SetExpectedDBChanges(SyncDataList(),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store_);
  verifier()->SetExpectedSyncChanges(sync_list);

  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form1));
  list.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form2));
  list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form3));
  service_->ActOnPasswordStoreChanges(list);
}

}  // namespace

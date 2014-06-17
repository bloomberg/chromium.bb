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
#include "components/password_manager/core/browser/mock_password_store.h"
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

namespace password_manager {

namespace {

typedef std::vector<SyncChange> SyncChangeList;

const sync_pb::PasswordSpecificsData& GetPasswordSpecifics(
    const syncer::SyncData& sync_data) {
  const sync_pb::EntitySpecifics& specifics = sync_data.GetSpecifics();
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
  EXPECT_EQ(expected_password.type(), actual_password.type());
  EXPECT_EQ(expected_password.times_used(), actual_password.times_used());
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

  std::string tag = MakePasswordSyncTag(*password_specifics);
  return syncer::SyncData::CreateLocalData(tag, tag, password_data);
}

SyncChange CreateSyncChange(const autofill::PasswordForm& password,
                            SyncChange::SyncChangeType type) {
  SyncData data = SyncDataFromPassword(password);
  return SyncChange(FROM_HERE, type, data);
}

class FormFinder {
 public:
  explicit FormFinder(const autofill::PasswordForm& form) : form_(form) {}
  ~FormFinder() {}

  bool operator()(const autofill::PasswordForm& form) const;

 private:
  const autofill::PasswordForm form_;
};

bool FormFinder::operator()(const autofill::PasswordForm& form) const {
  return form.origin == form_.origin &&
         form.username_element == form_.username_element &&
         form.username_value == form_.username_value &&
         form.password_element == form_.password_element &&
         form.signon_realm == form_.signon_realm;
}

// A testable implementation of the |PasswordSyncableService| that mocks
// out all interaction with the password database.
class MockPasswordSyncableService : public PasswordSyncableService {
 public:
  explicit MockPasswordSyncableService(PasswordStoreSync* password_store)
      : PasswordSyncableService(password_store) {}
  virtual ~MockPasswordSyncableService() {}

  MOCK_METHOD1(NotifyPasswordStoreOfLoginChanges,
               void (const PasswordStoreChangeList&));

  MOCK_METHOD1(StartSyncFlare, void(syncer::ModelType));
};

// Class to verify the arguments passed to |PasswordStore|.
class PasswordStoreDataVerifier {
 public:
  PasswordStoreDataVerifier() {}
  ~PasswordStoreDataVerifier() {
    EXPECT_TRUE(expected_db_add_changes_.empty());
    EXPECT_TRUE(expected_db_update_changes_.empty());
    EXPECT_TRUE(expected_db_delete_changes_.empty());
  }

  class TestSyncChangeProcessor;

  // Sets expected changes to the password database.
  void SetExpectedDBChanges(
      const SyncDataList& add_forms,
      const std::vector<autofill::PasswordForm*>& update_forms,
      const std::vector<autofill::PasswordForm*>& delete_forms,
      MockPasswordStore* password_store);
  // Sets expected changes to TestSyncChangeProcessor.
  void SetExpectedSyncChanges(SyncChangeList list);

 private:
  // Checks that |change_list| matches |expected_sync_change_list_|.
  SyncError TestSyncChanges(const SyncChangeList& change_list);

  // Verifies that the |password| is present in the |expected_db_add_changes_|
  // list. If found, |password| would be removed from
  // |expected_db_add_changes_| list.
  PasswordStoreChangeList VerifyAdd(const autofill::PasswordForm& password) {
    return VerifyChange(PasswordStoreChange::ADD, password,
                        &expected_db_add_changes_);
  }

  // Verifies that the |password| is present in the
  // |expected_db_update_changes_| list. If found, |password| would be removed
  // from |expected_db_update_changes_| list.
  PasswordStoreChangeList VerifyUpdate(const autofill::PasswordForm& password) {
    return VerifyChange(PasswordStoreChange::UPDATE, password,
                        &expected_db_update_changes_);
  }

  // Verifies that the |password| is present in the
  // |expected_db_delete_changes_| list. If found, |password| would be removed
  // from |expected_db_delete_changes_| list.
  PasswordStoreChangeList VerifyDelete(const autofill::PasswordForm& password) {
    return VerifyChange(PasswordStoreChange::REMOVE, password,
                        &expected_db_delete_changes_);
  }

  static PasswordStoreChangeList VerifyChange(
      PasswordStoreChange::Type type,
      const autofill::PasswordForm& password,
      std::vector<autofill::PasswordForm>* password_list);

  std::vector<autofill::PasswordForm> expected_db_add_changes_;
  std::vector<autofill::PasswordForm> expected_db_update_changes_;
  std::vector<autofill::PasswordForm> expected_db_delete_changes_;
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
    const std::vector<autofill::PasswordForm*>& delete_forms,
    MockPasswordStore* password_store) {
  DCHECK(expected_db_add_changes_.empty());
  DCHECK(expected_db_update_changes_.empty());
  DCHECK(password_store);

  for (SyncDataList::const_iterator it = add_forms.begin();
       it != add_forms.end(); ++it) {
    autofill::PasswordForm form;
    PasswordFromSpecifics(GetPasswordSpecifics(*it), &form);
    expected_db_add_changes_.push_back(form);
  }
  if (expected_db_add_changes_.empty()) {
    EXPECT_CALL(*password_store, AddLoginImpl(_)).Times(0);
  } else {
    EXPECT_CALL(*password_store, AddLoginImpl(_))
        .Times(expected_db_add_changes_.size())
        .WillRepeatedly(Invoke(this, &PasswordStoreDataVerifier::VerifyAdd));
  }

  for (std::vector<autofill::PasswordForm*>::const_iterator it =
           update_forms.begin();
       it != update_forms.end(); ++it) {
    expected_db_update_changes_.push_back(**it);
  }
  if (expected_db_update_changes_.empty()) {
    EXPECT_CALL(*password_store, UpdateLoginImpl(_)).Times(0);
  } else {
    EXPECT_CALL(*password_store, UpdateLoginImpl(_))
        .Times(expected_db_update_changes_.size())
        .WillRepeatedly(Invoke(this, &PasswordStoreDataVerifier::VerifyUpdate));
  }

  for (std::vector<autofill::PasswordForm*>::const_iterator it =
           delete_forms.begin();
       it != delete_forms.end(); ++it) {
    expected_db_delete_changes_.push_back(**it);
  }
  if (expected_db_delete_changes_.empty()) {
    EXPECT_CALL(*password_store, RemoveLoginImpl(_)).Times(0);
  } else {
    EXPECT_CALL(*password_store, RemoveLoginImpl(_))
        .Times(expected_db_delete_changes_.size())
        .WillRepeatedly(Invoke(this, &PasswordStoreDataVerifier::VerifyDelete));
  }
}

void PasswordStoreDataVerifier::SetExpectedSyncChanges(SyncChangeList list) {
  expected_sync_change_list_.swap(list);
}

SyncError PasswordStoreDataVerifier::TestSyncChanges(
    const SyncChangeList& change_list) {
  for (SyncChangeList::const_iterator it = change_list.begin();
      it != change_list.end(); ++it) {
    SyncData data = it->sync_data();
    std::string actual_tag = syncer::SyncDataLocal(data).GetTag();

    bool matched = false;
    for (SyncChangeList::iterator expected_it =
             expected_sync_change_list_.begin();
         expected_it != expected_sync_change_list_.end();
         ++expected_it) {
      const sync_pb::PasswordSpecificsData& expected_password(
          GetPasswordSpecifics(expected_it->sync_data()));
      if (actual_tag == MakePasswordSyncTag(expected_password)) {
        EXPECT_EQ(expected_it->change_type(), it->change_type());
        matched = true;
        if (it->change_type() != SyncChange::ACTION_DELETE)
          PasswordsEqual(expected_password, GetPasswordSpecifics(data));
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
  std::vector<autofill::PasswordForm>::iterator it = std::find_if(
      password_list->begin(), password_list->end(), FormFinder(password));
  EXPECT_NE(password_list->end(), it);
  PasswordsEqual(GetPasswordSpecifics(SyncDataFromPassword(*it)),
                 GetPasswordSpecifics(SyncDataFromPassword(password)));
  if (type != PasswordStoreChange::REMOVE) {
    EXPECT_FALSE(password.date_synced.is_null()) << password.signon_realm;
    EXPECT_FALSE(password.date_synced.is_max()) << password.signon_realm;
  }
  password_list->erase(it);
  return PasswordStoreChangeList(1, PasswordStoreChange(type, password));
}

class PasswordSyncableServiceWrapper {
 public:
  PasswordSyncableServiceWrapper() {
    password_store_ = new MockPasswordStore;
    service_.reset(new MockPasswordSyncableService(
        password_store_->GetSyncInterface()));
  }

  ~PasswordSyncableServiceWrapper() {
    password_store_->Shutdown();
  }

  MockPasswordStore* password_store() {
    return password_store_;
  }

  MockPasswordSyncableService* service() {
    return service_.get();
  }

  // Returnes the scoped_ptr to |service_| thus NULLing out it.
  scoped_ptr<syncer::SyncChangeProcessor> ReleaseSyncableService() {
    return service_.PassAs<syncer::SyncChangeProcessor>();
  }

  PasswordStoreDataVerifier* verifier() {
    return &verifier_;
  }

  scoped_ptr<syncer::SyncChangeProcessor> CreateSyncChangeProcessor() {
    return make_scoped_ptr<syncer::SyncChangeProcessor>(
        new PasswordStoreDataVerifier::TestSyncChangeProcessor(verifier()));
  }

  // Sets the data that will be returned to the caller accessing password store.
  void SetPasswordStoreData(
      const std::vector<autofill::PasswordForm*>& forms,
      const std::vector<autofill::PasswordForm*>& blacklist_forms) {
    EXPECT_CALL(*password_store_, FillAutofillableLogins(_))
        .WillOnce(Invoke(AppendVector(forms)))
        .RetiresOnSaturation();
    EXPECT_CALL(*password_store_, FillBlacklistLogins(_))
        .WillOnce(Invoke(AppendVector(blacklist_forms)))
        .RetiresOnSaturation();
  }

 protected:
  scoped_refptr<MockPasswordStore> password_store_;
  scoped_ptr<MockPasswordSyncableService> service_;
  PasswordStoreDataVerifier verifier_;

 private:
  struct AppendVector {
    explicit AppendVector(
        const std::vector<autofill::PasswordForm*>& append_forms)
        : append_forms_(append_forms) {
    }

    ~AppendVector() {}

    bool operator()(std::vector<autofill::PasswordForm*>* forms) const {
      forms->insert(forms->end(), append_forms_.begin(), append_forms_.end());
      return true;
    }

    std::vector<autofill::PasswordForm*> append_forms_;
  };

  DISALLOW_COPY_AND_ASSIGN(PasswordSyncableServiceWrapper);
};

class PasswordSyncableServiceTest : public testing::Test,
                                    public PasswordSyncableServiceWrapper {
 public:
  PasswordSyncableServiceTest() {}
  virtual ~PasswordSyncableServiceTest() {}
};


// Both sync and password db have data that are not present in the other.
TEST_F(PasswordSyncableServiceTest, AdditionsInBoth) {
  scoped_ptr<autofill::PasswordForm> form1(new autofill::PasswordForm);
  form1->signon_realm = "abc";
  std::vector<autofill::PasswordForm*> forms;
  forms.push_back(form1.release());
  SetPasswordStoreData(forms, std::vector<autofill::PasswordForm*>());

  SyncData sync_data = CreateSyncData("def");
  SyncDataList list;
  list.push_back(sync_data);

  verifier()->SetExpectedDBChanges(list,
                                   std::vector<autofill::PasswordForm*>(),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store());
  verifier()->SetExpectedSyncChanges(
      SyncChangeList(1, CreateSyncChange(*forms[0], SyncChange::ACTION_ADD)));
  EXPECT_CALL(*service(), NotifyPasswordStoreOfLoginChanges(_));

  service()->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                      list,
                                      CreateSyncChangeProcessor(),
                                      scoped_ptr<syncer::SyncErrorFactory>());
}

// Sync has data that is not present in the password db.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInSync) {
  SetPasswordStoreData(std::vector<autofill::PasswordForm*>(),
                       std::vector<autofill::PasswordForm*>());

  SyncData sync_data = CreateSyncData("def");
  SyncDataList list;
  list.push_back(sync_data);

  verifier()->SetExpectedDBChanges(list,
                                   std::vector<autofill::PasswordForm*>(),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store());
  verifier()->SetExpectedSyncChanges(SyncChangeList());
  EXPECT_CALL(*service(), NotifyPasswordStoreOfLoginChanges(_));

  service()->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                      list,
                                      CreateSyncChangeProcessor(),
                                      scoped_ptr<syncer::SyncErrorFactory>());
}

// Passwords db has data that is not present in sync.
TEST_F(PasswordSyncableServiceTest, AdditionOnlyInPasswordStore) {
  scoped_ptr<autofill::PasswordForm> form1(new autofill::PasswordForm);
  form1->signon_realm = "abc";
  form1->times_used = 2;
  form1->type = autofill::PasswordForm::TYPE_GENERATED;
  std::vector<autofill::PasswordForm*> forms;
  forms.push_back(form1.release());
  SetPasswordStoreData(forms, std::vector<autofill::PasswordForm*>());

  verifier()->SetExpectedDBChanges(SyncDataList(),
                                   std::vector<autofill::PasswordForm*>(),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store());
  verifier()->SetExpectedSyncChanges(
      SyncChangeList(1, CreateSyncChange(*forms[0], SyncChange::ACTION_ADD)));
  EXPECT_CALL(*service_,
              NotifyPasswordStoreOfLoginChanges(PasswordStoreChangeList()));

  service()->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                      SyncDataList(),
                                      CreateSyncChangeProcessor(),
                                      scoped_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync contain the same data.
TEST_F(PasswordSyncableServiceTest, BothInSync) {
  scoped_ptr<autofill::PasswordForm> form1(new autofill::PasswordForm);
  form1->signon_realm = "abc";
  form1->times_used = 3;
  form1->type = autofill::PasswordForm::TYPE_GENERATED;
  std::vector<autofill::PasswordForm*> forms;
  forms.push_back(form1.release());
  SetPasswordStoreData(forms, std::vector<autofill::PasswordForm*>());

  verifier()->SetExpectedDBChanges(SyncDataList(),
                                   std::vector<autofill::PasswordForm*>(),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store());
  verifier()->SetExpectedSyncChanges(SyncChangeList());
  EXPECT_CALL(*service_,
              NotifyPasswordStoreOfLoginChanges(PasswordStoreChangeList()));

  service()->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                      SyncDataList(1, CreateSyncData("abc")),
                                      CreateSyncChangeProcessor(),
                                      scoped_ptr<syncer::SyncErrorFactory>());
}

// Both passwords db and sync have the same data but they need to be merged
// as some fields of the data differ.
TEST_F(PasswordSyncableServiceTest, Merge) {
  scoped_ptr<autofill::PasswordForm> form1(new autofill::PasswordForm);
  form1->signon_realm = "abc";
  form1->action = GURL("http://pie.com");
  form1->date_created = base::Time::Now();
  form1->preferred = true;
  std::vector<autofill::PasswordForm*> forms;
  forms.push_back(form1.release());
  SetPasswordStoreData(forms, std::vector<autofill::PasswordForm*>());

  autofill::PasswordForm form2(*forms[0]);
  form2.preferred = false;
  verifier()->SetExpectedDBChanges(SyncDataList(),
                                   std::vector<autofill::PasswordForm*>(1,
                                                                        &form2),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store());
  verifier()->SetExpectedSyncChanges(SyncChangeList());

  EXPECT_CALL(*service(), NotifyPasswordStoreOfLoginChanges(_));

  service()->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                      SyncDataList(1,
                                                   SyncDataFromPassword(form2)),
                                      CreateSyncChangeProcessor(),
                                      scoped_ptr<syncer::SyncErrorFactory>());
}

// Initiate sync due to local DB changes.
TEST_F(PasswordSyncableServiceTest, PasswordStoreChanges) {
  // Set the sync change processor first.
  SetPasswordStoreData(std::vector<autofill::PasswordForm*>(),
                       std::vector<autofill::PasswordForm*>());
  verifier()->SetExpectedSyncChanges(SyncChangeList());
  EXPECT_CALL(*service_,
              NotifyPasswordStoreOfLoginChanges(PasswordStoreChangeList()));
  service_->MergeDataAndStartSyncing(syncer::PASSWORDS,
                                     SyncDataList(),
                                     CreateSyncChangeProcessor(),
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
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store());
  verifier()->SetExpectedSyncChanges(sync_list);

  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form1));
  list.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form2));
  list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form3));
  service()->ActOnPasswordStoreChanges(list);
}

// Process all types of changes from sync.
TEST_F(PasswordSyncableServiceTest, ProcessSyncChanges) {
  autofill::PasswordForm updated_form;
  updated_form.signon_realm = "abc";
  updated_form.action = GURL("http://foo.com");
  updated_form.date_created = base::Time::Now();
  autofill::PasswordForm deleted_form;
  deleted_form.signon_realm = "xyz";
  deleted_form.action = GURL("http://bar.com");
  deleted_form.blacklisted_by_user = true;

  SyncData add_data = CreateSyncData("def");
  std::vector<autofill::PasswordForm*> updated_passwords(1, &updated_form);
  std::vector<autofill::PasswordForm*> deleted_passwords(1, &deleted_form);
  verifier()->SetExpectedDBChanges(SyncDataList(1, add_data),
                                   updated_passwords,
                                   deleted_passwords,
                                   password_store());

  SyncChangeList list;
  list.push_back(SyncChange(FROM_HERE,
                            syncer::SyncChange::ACTION_ADD,
                            add_data));
  list.push_back(CreateSyncChange(updated_form,
                                  syncer::SyncChange::ACTION_UPDATE));
  list.push_back(CreateSyncChange(deleted_form,
                                  syncer::SyncChange::ACTION_DELETE));
  EXPECT_CALL(*service(), NotifyPasswordStoreOfLoginChanges(_));
  service()->ProcessSyncChanges(FROM_HERE, list);
}

// Retrives sync data from the model.
TEST_F(PasswordSyncableServiceTest, GetAllSyncData) {
  scoped_ptr<autofill::PasswordForm> form1(new autofill::PasswordForm);
  form1->signon_realm = "abc";
  form1->action = GURL("http://foo.com");
  form1->times_used = 5;
  form1->type = autofill::PasswordForm::TYPE_GENERATED;
  scoped_ptr<autofill::PasswordForm> form2(new autofill::PasswordForm);
  form2->signon_realm = "xyz";
  form2->action = GURL("http://bar.com");
  form2->blacklisted_by_user = true;
  std::vector<autofill::PasswordForm*> forms(1, form1.release());
  std::vector<autofill::PasswordForm*> blacklist_forms(1, form2.release());
  SetPasswordStoreData(forms, blacklist_forms);

  SyncDataList expected_list;
  expected_list.push_back(SyncDataFromPassword(*forms[0]));
  expected_list.push_back(SyncDataFromPassword(*blacklist_forms[0]));

  verifier()->SetExpectedDBChanges(SyncDataList(),
                                   std::vector<autofill::PasswordForm*>(),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store());

  SyncDataList actual_list = service()->GetAllSyncData(syncer::PASSWORDS);
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (SyncDataList::iterator i(actual_list.begin()), j(expected_list.begin());
       i != actual_list.end() && j != expected_list.end(); ++i, ++j) {
    PasswordsEqual(GetPasswordSpecifics(*j), GetPasswordSpecifics(*i));
  }
}

// Creates 2 PasswordSyncableService instances, merges the content of the first
// one to the second one and back.
TEST_F(PasswordSyncableServiceTest, MergeDataAndPushBack) {
  scoped_ptr<autofill::PasswordForm> form1(new autofill::PasswordForm);
  form1->signon_realm = "abc";
  form1->action = GURL("http://foo.com");
  std::vector<autofill::PasswordForm*> forms(1, form1.release());
  SetPasswordStoreData(forms, std::vector<autofill::PasswordForm*>());

  PasswordSyncableServiceWrapper other_service_wrapper;
  scoped_ptr<autofill::PasswordForm> form2(new autofill::PasswordForm);
  form2->signon_realm = "xyz";
  form2->action = GURL("http://bar.com");
  syncer::SyncData form2_sync_data = SyncDataFromPassword(*form2);
  other_service_wrapper.SetPasswordStoreData(
      std::vector<autofill::PasswordForm*>(1, form2.release()),
      std::vector<autofill::PasswordForm*>());

  verifier()->SetExpectedDBChanges(SyncDataList(1, form2_sync_data),
                                   std::vector<autofill::PasswordForm*>(),
                                   std::vector<autofill::PasswordForm*>(),
                                   password_store());
  other_service_wrapper.verifier()->SetExpectedDBChanges(
      SyncDataList(1, SyncDataFromPassword(*forms[0])),
      std::vector<autofill::PasswordForm*>(),
      std::vector<autofill::PasswordForm*>(),
      other_service_wrapper.password_store());
  EXPECT_CALL(*service(), NotifyPasswordStoreOfLoginChanges(_));
  EXPECT_CALL(*other_service_wrapper.service(),
              NotifyPasswordStoreOfLoginChanges(_));

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
  form.signon_realm = "abc";
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

}  // namespace

}  // namespace password_manager

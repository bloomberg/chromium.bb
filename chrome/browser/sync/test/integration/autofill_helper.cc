// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/autofill_helper.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database.h"

using autofill::AutofillChangeList;
using autofill::AutofillEntry;
using autofill::AutofillKey;
using autofill::AutofillProfile;
using autofill::AutofillTable;
using autofill::AutofillType;
using autofill::AutofillWebDataService;
using autofill::AutofillWebDataServiceObserverOnDBThread;
using autofill::CreditCard;
using autofill::FormFieldData;
using autofill::PersonalDataManager;
using autofill::PersonalDataManagerObserver;
using base::WaitableEvent;
using content::BrowserThread;
using sync_datatype_helper::test;
using testing::_;

namespace {

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class MockWebDataServiceObserver
    : public AutofillWebDataServiceObserverOnDBThread {
 public:
  MOCK_METHOD1(AutofillEntriesChanged,
               void(const AutofillChangeList& changes));
};

class MockPersonalDataManagerObserver : public PersonalDataManagerObserver {
 public:
  MOCK_METHOD0(OnPersonalDataChanged, void());
};

void RunOnDBThreadAndSignal(base::Closure task,
                            base::WaitableEvent* done_event) {
  if (!task.is_null()) {
    task.Run();
  }
  done_event->Signal();
}

void RunOnDBThreadAndBlock(base::Closure task) {
  WaitableEvent done_event(false, false);
  BrowserThread::PostTask(BrowserThread::DB,
                          FROM_HERE,
                          Bind(&RunOnDBThreadAndSignal, task, &done_event));
  done_event.Wait();
}

void RemoveKeyDontBlockForSync(int profile, const AutofillKey& key) {
  WaitableEvent done_event(false, false);

  MockWebDataServiceObserver mock_observer;
  EXPECT_CALL(mock_observer, AutofillEntriesChanged(_))
      .WillOnce(SignalEvent(&done_event));

  scoped_refptr<AutofillWebDataService> wds =
      autofill_helper::GetWebDataService(profile);

  void(AutofillWebDataService::*add_observer_func)(
      AutofillWebDataServiceObserverOnDBThread*) =
      &AutofillWebDataService::AddObserver;
  RunOnDBThreadAndBlock(Bind(add_observer_func, wds, &mock_observer));

  wds->RemoveFormValueForElementName(key.name(), key.value());
  done_event.Wait();

  void(AutofillWebDataService::*remove_observer_func)(
      AutofillWebDataServiceObserverOnDBThread*) =
      &AutofillWebDataService::RemoveObserver;
  RunOnDBThreadAndBlock(Bind(remove_observer_func, wds, &mock_observer));
}

void GetAllAutofillEntriesOnDBThread(AutofillWebDataService* wds,
                                     std::vector<AutofillEntry>* entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  AutofillTable::FromWebDatabase(
      wds->GetDatabase())->GetAllAutofillEntries(entries);
}

std::vector<AutofillEntry> GetAllAutofillEntries(AutofillWebDataService* wds) {
  std::vector<AutofillEntry> entries;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RunOnDBThreadAndBlock(Bind(&GetAllAutofillEntriesOnDBThread,
                             Unretained(wds),
                             &entries));
  return entries;
}

// UI thread returns from the update operations on the DB thread and schedules
// the sync. This function blocks until after this scheduled sync is complete by
// scheduling additional empty task on DB Thread. Call after AddKeys/RemoveKey.
void BlockForPendingDBThreadTasks() {
  // The order of the notifications is undefined, so sync change sometimes is
  // posted after the notification for observer_helper. Post new task to db
  // thread that guaranteed to be after sync and would be blocking until
  // completion.
  RunOnDBThreadAndBlock(base::Closure());
}

}  // namespace

namespace autofill_helper {

AutofillProfile CreateAutofillProfile(ProfileType type) {
  AutofillProfile profile;
  switch (type) {
    case PROFILE_MARION:
      autofill::test::SetProfileInfoWithGuid(&profile,
          "C837507A-6C3B-4872-AC14-5113F157D668",
          "Marion", "Mitchell", "Morrison",
          "johnwayne@me.xyz", "Fox",
          "123 Zoo St.", "unit 5", "Hollywood", "CA",
          "91601", "US", "12345678910");
      break;
    case PROFILE_HOMER:
      autofill::test::SetProfileInfoWithGuid(&profile,
          "137DE1C3-6A30-4571-AC86-109B1ECFBE7F",
          "Homer", "J.", "Simpson",
          "homer@abc.com", "SNPP",
          "1 Main St", "PO Box 1", "Springfield", "MA",
          "94101", "US", "14155551212");
      break;
    case PROFILE_FRASIER:
      autofill::test::SetProfileInfoWithGuid(&profile,
          "9A5E6872-6198-4688-BF75-0016E781BB0A",
          "Frasier", "Winslow", "Crane",
          "", "randomness", "", "Apt. 4", "Seattle", "WA",
          "99121", "US", "0000000000");
      break;
    case PROFILE_NULL:
      autofill::test::SetProfileInfoWithGuid(&profile,
          "FE461507-7E13-4198-8E66-74C7DB6D8322",
          "", "", "", "", "", "", "", "", "", "", "", "");
      break;
  }
  return profile;
}

scoped_refptr<AutofillWebDataService> GetWebDataService(int index) {
  return WebDataServiceFactory::GetAutofillWebDataForProfile(
      test()->GetProfile(index), Profile::EXPLICIT_ACCESS);
}

PersonalDataManager* GetPersonalDataManager(int index) {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      test()->GetProfile(index));
}

void AddKeys(int profile, const std::set<AutofillKey>& keys) {
  std::vector<FormFieldData> form_fields;
  for (std::set<AutofillKey>::const_iterator i = keys.begin();
       i != keys.end();
       ++i) {
    FormFieldData field;
    field.name = i->name();
    field.value = i->value();
    form_fields.push_back(field);
  }

  WaitableEvent done_event(false, false);
  MockWebDataServiceObserver mock_observer;
  EXPECT_CALL(mock_observer, AutofillEntriesChanged(_))
      .WillOnce(SignalEvent(&done_event));

  scoped_refptr<AutofillWebDataService> wds = GetWebDataService(profile);

  void(AutofillWebDataService::*add_observer_func)(
      AutofillWebDataServiceObserverOnDBThread*) =
      &AutofillWebDataService::AddObserver;
  RunOnDBThreadAndBlock(Bind(add_observer_func, wds, &mock_observer));

  wds->AddFormFields(form_fields);
  done_event.Wait();
  BlockForPendingDBThreadTasks();

  void(AutofillWebDataService::*remove_observer_func)(
      AutofillWebDataServiceObserverOnDBThread*) =
      &AutofillWebDataService::RemoveObserver;
  RunOnDBThreadAndBlock(Bind(remove_observer_func, wds, &mock_observer));
}

void RemoveKey(int profile, const AutofillKey& key) {
  RemoveKeyDontBlockForSync(profile, key);
  BlockForPendingDBThreadTasks();
}

void RemoveKeys(int profile) {
  std::set<AutofillEntry> keys = GetAllKeys(profile);
  for (std::set<AutofillEntry>::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    RemoveKeyDontBlockForSync(profile, it->key());
  }
  BlockForPendingDBThreadTasks();
}

std::set<AutofillEntry> GetAllKeys(int profile) {
  scoped_refptr<AutofillWebDataService> wds = GetWebDataService(profile);
  std::vector<AutofillEntry> all_entries = GetAllAutofillEntries(wds.get());
  std::set<AutofillEntry> all_keys;
  for (std::vector<AutofillEntry>::const_iterator it = all_entries.begin();
       it != all_entries.end(); ++it) {
    all_keys.insert(*it);
  }
  return all_keys;
}

bool KeysMatch(int profile_a, int profile_b) {
  return GetAllKeys(profile_a) == GetAllKeys(profile_b);
}

namespace {

class KeysMatchStatusChecker : public MultiClientStatusChangeChecker {
 public:
  KeysMatchStatusChecker(int profile_a, int profile_b);
  virtual ~KeysMatchStatusChecker();

  virtual bool IsExitConditionSatisfied() OVERRIDE;
  virtual std::string GetDebugMessage() const OVERRIDE;

 private:
  const int profile_a_;
  const int profile_b_;
};

KeysMatchStatusChecker::KeysMatchStatusChecker(int profile_a, int profile_b)
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()),
      profile_a_(profile_a),
      profile_b_(profile_b) {
}

KeysMatchStatusChecker::~KeysMatchStatusChecker() {
}

bool KeysMatchStatusChecker::IsExitConditionSatisfied() {
  return KeysMatch(profile_a_, profile_b_);
}

std::string KeysMatchStatusChecker::GetDebugMessage() const {
  return "Waiting for matching autofill keys";
}

}  // namespace

bool AwaitKeysMatch(int a, int b) {
  KeysMatchStatusChecker checker(a, b);
  checker.Wait();
  return !checker.TimedOut();
}

void SetProfiles(int profile, std::vector<AutofillProfile>* autofill_profiles) {
  MockPersonalDataManagerObserver observer;
  EXPECT_CALL(observer, OnPersonalDataChanged()).
      WillOnce(QuitUIMessageLoop());
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  pdm->AddObserver(&observer);
  pdm->SetProfiles(autofill_profiles);
  base::MessageLoop::current()->Run();
  pdm->RemoveObserver(&observer);
}

void SetCreditCards(int profile, std::vector<CreditCard>* credit_cards) {
  MockPersonalDataManagerObserver observer;
  EXPECT_CALL(observer, OnPersonalDataChanged()).
      WillOnce(QuitUIMessageLoop());
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  pdm->AddObserver(&observer);
  pdm->SetCreditCards(credit_cards);
  base::MessageLoop::current()->Run();
  pdm->RemoveObserver(&observer);
}

void AddProfile(int profile, const AutofillProfile& autofill_profile) {
  const std::vector<AutofillProfile*>& all_profiles = GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i)
    autofill_profiles.push_back(*all_profiles[i]);
  autofill_profiles.push_back(autofill_profile);
  autofill_helper::SetProfiles(profile, &autofill_profiles);
}

void RemoveProfile(int profile, const std::string& guid) {
  const std::vector<AutofillProfile*>& all_profiles = GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    if (all_profiles[i]->guid() != guid)
      autofill_profiles.push_back(*all_profiles[i]);
  }
  autofill_helper::SetProfiles(profile, &autofill_profiles);
}

void UpdateProfile(int profile,
                   const std::string& guid,
                   const AutofillType& type,
                   const base::string16& value) {
  const std::vector<AutofillProfile*>& all_profiles = GetAllProfiles(profile);
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    profiles.push_back(*all_profiles[i]);
    if (all_profiles[i]->guid() == guid)
      profiles.back().SetRawInfo(type.GetStorableType(), value);
  }
  autofill_helper::SetProfiles(profile, &profiles);
}

const std::vector<AutofillProfile*>& GetAllProfiles(
    int profile) {
  MockPersonalDataManagerObserver observer;
  EXPECT_CALL(observer, OnPersonalDataChanged()).
      WillOnce(QuitUIMessageLoop());
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  pdm->AddObserver(&observer);
  pdm->Refresh();
  base::MessageLoop::current()->Run();
  pdm->RemoveObserver(&observer);
  return pdm->web_profiles();
}

int GetProfileCount(int profile) {
  return GetAllProfiles(profile).size();
}

int GetKeyCount(int profile) {
  return GetAllKeys(profile).size();
}

namespace {

bool ProfilesMatchImpl(
    int profile_a,
    const std::vector<AutofillProfile*>& autofill_profiles_a,
    int profile_b,
    const std::vector<AutofillProfile*>& autofill_profiles_b) {
  std::map<std::string, AutofillProfile> autofill_profiles_a_map;
  for (size_t i = 0; i < autofill_profiles_a.size(); ++i) {
    const AutofillProfile* p = autofill_profiles_a[i];
    autofill_profiles_a_map[p->guid()] = *p;
  }

  for (size_t i = 0; i < autofill_profiles_b.size(); ++i) {
    const AutofillProfile* p = autofill_profiles_b[i];
    if (!autofill_profiles_a_map.count(p->guid())) {
      DVLOG(1) << "GUID " << p->guid() << " not found in profile " << profile_b
               << ".";
      return false;
    }
    AutofillProfile* expected_profile = &autofill_profiles_a_map[p->guid()];
    expected_profile->set_guid(p->guid());
    if (*expected_profile != *p) {
      DVLOG(1) << "Mismatch in profile with GUID " << p->guid() << ".";
      return false;
    }
    autofill_profiles_a_map.erase(p->guid());
  }

  if (autofill_profiles_a_map.size()) {
    DVLOG(1) << "Entries present in Profile " << profile_a << " but not in "
             << profile_b << ".";
    return false;
  }
  return true;
}

}  // namespace

bool ProfilesMatch(int profile_a, int profile_b) {
  const std::vector<AutofillProfile*>& autofill_profiles_a =
      GetAllProfiles(profile_a);
  const std::vector<AutofillProfile*>& autofill_profiles_b =
      GetAllProfiles(profile_b);
  return ProfilesMatchImpl(
      profile_a, autofill_profiles_a, profile_b, autofill_profiles_b);
}

bool AllProfilesMatch() {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!ProfilesMatch(0, i)) {
      DVLOG(1) << "Profile " << i << "does not contain the same autofill "
                                     "profiles as profile 0.";
      return false;
    }
  }
  return true;
}

namespace {

class ProfilesMatchStatusChecker : public StatusChangeChecker,
                                   public PersonalDataManagerObserver {
 public:
  ProfilesMatchStatusChecker(int profile_a, int profile_b);
  virtual ~ProfilesMatchStatusChecker();

  // StatusChangeChecker implementation.
  virtual bool IsExitConditionSatisfied() OVERRIDE;
  virtual std::string GetDebugMessage() const OVERRIDE;

  // PersonalDataManager implementation.
  virtual void OnPersonalDataChanged() OVERRIDE;

  // Wait for conidtion to beome true.
  void Wait();

 private:
  const int profile_a_;
  const int profile_b_;
  bool registered_;
};

ProfilesMatchStatusChecker::ProfilesMatchStatusChecker(int profile_a,
                                                       int profile_b)
    : profile_a_(profile_a), profile_b_(profile_b), registered_(false) {
}

ProfilesMatchStatusChecker::~ProfilesMatchStatusChecker() {
  PersonalDataManager* pdm_a = GetPersonalDataManager(profile_a_);
  PersonalDataManager* pdm_b = GetPersonalDataManager(profile_b_);
  if (registered_) {
    pdm_a->RemoveObserver(this);
    pdm_b->RemoveObserver(this);
  }
}

bool ProfilesMatchStatusChecker::IsExitConditionSatisfied() {
  PersonalDataManager* pdm_a = GetPersonalDataManager(profile_a_);
  PersonalDataManager* pdm_b = GetPersonalDataManager(profile_b_);

  const std::vector<AutofillProfile*>& autofill_profiles_a =
      pdm_a->web_profiles();
  const std::vector<AutofillProfile*>& autofill_profiles_b =
      pdm_b->web_profiles();

  return ProfilesMatchImpl(
      profile_a_, autofill_profiles_a, profile_b_, autofill_profiles_b);
}

void ProfilesMatchStatusChecker::Wait() {
  PersonalDataManager* pdm_a = GetPersonalDataManager(profile_a_);
  PersonalDataManager* pdm_b = GetPersonalDataManager(profile_b_);

  pdm_a->AddObserver(this);
  pdm_b->AddObserver(this);

  pdm_a->Refresh();
  pdm_b->Refresh();

  registered_ = true;

  StartBlockingWait();
}

std::string ProfilesMatchStatusChecker::GetDebugMessage() const {
  return "Waiting for matching autofill profiles";
}

void ProfilesMatchStatusChecker::OnPersonalDataChanged() {
  CheckExitCondition();
}

}  // namespace

bool AwaitProfilesMatch(int a, int b) {
  ProfilesMatchStatusChecker checker(a, b);
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace autofill_helper

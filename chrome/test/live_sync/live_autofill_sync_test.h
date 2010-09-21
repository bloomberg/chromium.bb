// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_AUTOFILL_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_AUTOFILL_SYNC_TEST_H_
#pragma once

#include <map>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/thread_observer_helper.h"

using base::WaitableEvent;
using testing::_;

namespace {
// Define these << operators so we can use EXPECT_EQ with the
// AutofillKeys type.
template<class T1, class T2, class T3>
std::ostream& operator<<(std::ostream& os, const std::set<T1, T2, T3>& seq) {
  typedef typename std::set<T1, T2, T3>::const_iterator SetConstIterator;
  for (SetConstIterator i = seq.begin(); i != seq.end(); ++i) {
    os << *i << ", ";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const AutofillKey& key) {
  return os << UTF16ToUTF8(key.name()) << ", " << UTF16ToUTF8(key.value());
}

class GetAllAutofillEntries
    : public base::RefCountedThreadSafe<GetAllAutofillEntries> {
 public:
  explicit GetAllAutofillEntries(WebDataService* web_data_service)
      : web_data_service_(web_data_service),
        done_event_(false, false) {}

  void Init() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    ChromeThread::PostTask(
        ChromeThread::DB,
        FROM_HERE,
        NewRunnableMethod(this, &GetAllAutofillEntries::Run));
    done_event_.Wait();
  }

  const std::vector<AutofillEntry>& entries() const {
    return entries_;
  }

 private:
  friend class base::RefCountedThreadSafe<GetAllAutofillEntries>;

  void Run() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
    web_data_service_->GetDatabase()->GetAllAutofillEntries(&entries_);
    done_event_.Signal();
  }

  WebDataService* web_data_service_;
  base::WaitableEvent done_event_;
  std::vector<AutofillEntry> entries_;
};

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class AutofillDBThreadObserverHelper : public DBThreadObserverHelper {
 protected:
  virtual void RegisterObservers() {
    registrar_.Add(&observer_,
                   NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   NotificationService::AllSources());
    registrar_.Add(&observer_,
                   NotificationType::AUTOFILL_PROFILE_CHANGED,
                   NotificationService::AllSources());
  }
};

enum ProfileType {
  PROFILE_MARION,
  PROFILE_HOMER,
  PROFILE_FRASIER,
  PROFILE_NULL
};

void FillProfile(ProfileType type, AutoFillProfile* profile) {
  switch (type) {
    case PROFILE_MARION:
      autofill_unittest::SetProfileInfo(profile,
        "Billing", "Marion", "Mitchell", "Morrison",
        "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
        "91601", "US", "12345678910", "01987654321");
      break;
    case PROFILE_HOMER:
      autofill_unittest::SetProfileInfo(profile,
        "Shipping", "Homer", "J.", "Simpson",
        "homer@snpp.com", "SNPP", "1 Main St", "PO Box 1", "Springfield", "MA",
        "94101", "US", "14155551212", "14155551313");
      break;
    case PROFILE_FRASIER:
      autofill_unittest::SetProfileInfo(profile,
        "Business", "Frasier", "Winslow", "Crane",
        "", "randomness", "", "Apt. 4", "Seattle", "WA",
        "99121", "US", "0000000000", "ABCDEFGHIJK");
    case PROFILE_NULL:
      autofill_unittest::SetProfileInfo(profile,
        "", "key", "", "", "", "", "", "", "", "", "", "", "", "");
      break;
  }
}

class MockPersonalDataManagerObserver : public PersonalDataManager::Observer {
 public:
  MOCK_METHOD0(OnPersonalDataLoaded, void());
};

}  // namespace

class LiveAutofillSyncTest : public LiveSyncTest {
 public:
  typedef std::set<AutofillKey> AutofillKeys;
  typedef std::vector<AutoFillProfile*> AutoFillProfiles;

  explicit LiveAutofillSyncTest(TestType test_type)
      : LiveSyncTest(test_type) {}
  virtual ~LiveAutofillSyncTest() {}

  // Used to access the web data service within a particular sync profile.
  WebDataService* GetWebDataService(int index) {
    return GetProfile(index)->GetWebDataService(Profile::EXPLICIT_ACCESS);
  }

  // Used to access the personal data manager within a particular sync profile.
  PersonalDataManager* GetPersonalDataManager(int index) {
    return GetProfile(index)->GetPersonalDataManager();
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableSyncAutofill);
  }

  void AddFormFieldsToWebData(WebDataService* wds, const AutofillKeys& keys) {
    std::vector<webkit_glue::FormField> form_fields;
    for (AutofillKeys::const_iterator i = keys.begin(); i != keys.end(); ++i) {
      form_fields.push_back(
          webkit_glue::FormField(string16(),
                                 (*i).name(),
                                 (*i).value(),
                                 string16(),
                                 0));
    }

    WaitableEvent done_event(false, false);
    scoped_refptr<AutofillDBThreadObserverHelper> observer_helper(
        new AutofillDBThreadObserverHelper());
    observer_helper->Init();

    EXPECT_CALL(*observer_helper->observer(), Observe(_, _, _)).
        WillOnce(SignalEvent(&done_event));
    wds->AddFormFields(form_fields);
    done_event.Wait();
  }

  void RemoveKeyFromWebData(WebDataService* wds, const AutofillKey& key) {
    WaitableEvent done_event(false, false);
    scoped_refptr<AutofillDBThreadObserverHelper> observer_helper(
        new AutofillDBThreadObserverHelper());
    observer_helper->Init();

    EXPECT_CALL(*observer_helper->observer(), Observe(_, _, _)).
        WillOnce(SignalEvent(&done_event));
    wds->RemoveFormValueForElementName(key.name(), key.value());
    done_event.Wait();
  }

  void SetProfiles(PersonalDataManager* pdm,
                   std::vector<AutoFillProfile>* profiles) {
    MockPersonalDataManagerObserver observer;
    EXPECT_CALL(observer, OnPersonalDataLoaded()).
        WillOnce(QuitUIMessageLoop());
    pdm->SetObserver(&observer);
    pdm->SetProfiles(profiles);
    MessageLoop::current()->Run();
    pdm->RemoveObserver(&observer);
  }

  void AddProfile(PersonalDataManager* pdm, const AutoFillProfile& profile) {
    const AutoFillProfiles& all_profiles = GetAllAutoFillProfiles(pdm);
    std::vector<AutoFillProfile> profiles;
    for (size_t i = 0; i < all_profiles.size(); ++i)
      profiles.push_back(*all_profiles[i]);
    profiles.push_back(profile);
    SetProfiles(pdm, &profiles);
  }

  void RemoveProfile(PersonalDataManager* pdm, const string16& label) {
    const AutoFillProfiles& all_profiles = GetAllAutoFillProfiles(pdm);
    std::vector<AutoFillProfile> profiles;
    for (size_t i = 0; i < all_profiles.size(); ++i) {
      if (all_profiles[i]->Label() != label)
        profiles.push_back(*all_profiles[i]);
    }
    SetProfiles(pdm, &profiles);
  }

  void UpdateProfile(PersonalDataManager* pdm,
                     const string16& label,
                     const AutoFillType& type,
                     const string16& value) {
    const AutoFillProfiles& all_profiles = GetAllAutoFillProfiles(pdm);
    std::vector<AutoFillProfile> profiles;
    for (size_t i = 0; i < all_profiles.size(); ++i) {
      profiles.push_back(*all_profiles[i]);
      if (all_profiles[i]->Label() == label)
        profiles.back().SetInfo(type, value);
    }
    SetProfiles(pdm, &profiles);
  }

  void GetAllAutofillKeys(WebDataService* wds, AutofillKeys* keys) {
    scoped_refptr<GetAllAutofillEntries> get_all_entries =
        new GetAllAutofillEntries(wds);
    get_all_entries->Init();
    const std::vector<AutofillEntry>& entries = get_all_entries->entries();

    for (size_t i = 0; i < entries.size(); ++i) {
      keys->insert(entries[i].key());
    }
  }

  const AutoFillProfiles& GetAllAutoFillProfiles(PersonalDataManager* pdm) {
    MockPersonalDataManagerObserver observer;
    EXPECT_CALL(observer, OnPersonalDataLoaded()).
        WillOnce(QuitUIMessageLoop());
    pdm->SetObserver(&observer);
    pdm->Refresh();
    MessageLoop::current()->Run();
    pdm->RemoveObserver(&observer);
    return pdm->profiles();
  }

  bool CompareAutoFillProfiles(const AutoFillProfiles& expected_profiles,
                               const AutoFillProfiles& profiles) {
    std::map<string16, AutoFillProfile> expected_profiles_map;
    for (size_t i = 0; i < expected_profiles.size(); ++i) {
      const AutoFillProfile* p = expected_profiles[i];
      expected_profiles_map[p->Label()] = *p;
    }

    for (size_t i = 0; i < profiles.size(); ++i) {
      const AutoFillProfile* p = profiles[i];
      if (!expected_profiles_map.count(p->Label())) {
        LOG(INFO) << "Label " << p->Label() << " not in expected";
        return false;
      }
      AutoFillProfile* expected_profile = &expected_profiles_map[p->Label()];
      expected_profile->set_unique_id(p->unique_id());
      if (*expected_profile != *p) {
        LOG(INFO) << "Profile mismatch";
        return false;
      }
      expected_profiles_map.erase(p->Label());
    }

    if (expected_profiles_map.size()) {
      LOG(INFO) << "Labels in expected but not supplied";
      return false;
    }

    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveAutofillSyncTest);
};

class TwoClientLiveAutofillSyncTest : public LiveAutofillSyncTest {
 public:
  TwoClientLiveAutofillSyncTest() : LiveAutofillSyncTest(TWO_CLIENT) {}
  ~TwoClientLiveAutofillSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveAutofillSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_AUTOFILL_SYNC_TEST_H_

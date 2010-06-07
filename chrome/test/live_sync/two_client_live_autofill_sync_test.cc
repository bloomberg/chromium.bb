// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/ref_counted.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/waitable_event.h"
#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_data_service_test_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/profile_sync_service_test_harness.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/thread_observer_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/glue/form_field.h"

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
  MARION,
  HOMER
};

void FillProfile(ProfileType type, AutoFillProfile* profile) {
  switch (type) {
    case MARION:
      autofill_unittest::SetProfileInfo(profile,
        "Billing", "Marion", "Mitchell", "Morrison",
        "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
        "91601", "US", "12345678910", "01987654321");
      break;
    case HOMER:
      autofill_unittest::SetProfileInfo(profile,
        "Shipping", "Homer", "J.", "Simpson",
        "homer@snpp.com", "SNPP", "1 Main St", "PO Box 1", "Springfield", "MA",
        "94101", "US", "14155551212", "14155551313");
      break;
  }
}

class MockPersonalDataManagerObserver : public PersonalDataManager::Observer {
 public:
  MOCK_METHOD0(OnPersonalDataLoaded, void());
};

}  // namespace

class TwoClientLiveAutofillSyncTest : public LiveSyncTest {
 public:
  typedef std::set<AutofillKey> AutofillKeys;
  typedef std::vector<AutoFillProfile*> AutoFillProfiles;
  TwoClientLiveAutofillSyncTest()
      : name1_(ASCIIToUTF16("name1")),
        name2_(ASCIIToUTF16("name2")),
        value1_(ASCIIToUTF16("value1")),
        value2_(ASCIIToUTF16("value2")),
        done_event1_(false, false),
        done_event2_(false, false) {
    // This makes sure browser is visible and active while running test.
    InProcessBrowserTest::set_show_window(true);
    // Set the initial timeout value to 5 min.
    InProcessBrowserTest::SetInitialTimeoutInMS(300000);
  }
  ~TwoClientLiveAutofillSyncTest() {}

 protected:
  void SetupHarness() {
    client1_.reset(new ProfileSyncServiceTestHarness(
        browser()->profile(), username_, password_));
    profile2_.reset(MakeProfile(FILE_PATH_LITERAL("client2")));
    client2_.reset(new ProfileSyncServiceTestHarness(
        profile2_.get(), username_, password_));
    wds1_ = browser()->profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
    wds2_ = profile2_->GetWebDataService(Profile::EXPLICIT_ACCESS);
    pdm1_ = browser()->profile()->GetPersonalDataManager();
    pdm2_ = profile2_->GetPersonalDataManager();
  }

  void SetupSync() {
    EXPECT_TRUE(client1_->SetupSync());
    EXPECT_TRUE(client1_->AwaitSyncCycleCompletion("Initial setup 1"));
    EXPECT_TRUE(client2_->SetupSync());
    EXPECT_TRUE(client2_->AwaitSyncCycleCompletion("Initial setup 2"));
  }

  void Cleanup() {
    client2_.reset();
    profile2_.reset();
  }

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

  ProfileSyncServiceTestHarness* client1() { return client1_.get(); }
  ProfileSyncServiceTestHarness* client2() { return client2_.get(); }

  PrefService* prefs1() { return browser()->profile()->GetPrefs(); }
  PrefService* prefs2() { return profile2_->GetPrefs(); }

  string16 name1_;
  string16 name2_;
  string16 value1_;
  string16 value2_;
  base::WaitableEvent done_event1_;
  base::WaitableEvent done_event2_;

  scoped_ptr<ProfileSyncServiceTestHarness> client1_;
  scoped_ptr<ProfileSyncServiceTestHarness> client2_;
  scoped_ptr<Profile> profile2_;
  WebDataService* wds1_;
  WebDataService* wds2_;
  PersonalDataManager* pdm1_;
  PersonalDataManager* pdm2_;

  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveAutofillSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, Client1HasData) {
  SetupHarness();

  AutofillKeys keys;
  keys.insert(AutofillKey("name1", "value1"));
  keys.insert(AutofillKey("name1", "value2"));
  keys.insert(AutofillKey("name2", "value3"));
  keys.insert(AutofillKey(WideToUTF16(L"Sigur R\u00F3s"),
                          WideToUTF16(L"\u00C1g\u00E6tis byrjun")));
  AddFormFieldsToWebData(wds1_, keys);

  SetupSync();

  AutofillKeys wd2_keys;
  GetAllAutofillKeys(wds2_, &wd2_keys);
  EXPECT_EQ(keys, wd2_keys);

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, BothHaveData) {
  SetupHarness();

  AutofillKeys keys1;
  keys1.insert(AutofillKey("name1", "value1"));
  keys1.insert(AutofillKey("name1", "value2"));
  keys1.insert(AutofillKey("name2", "value3"));
  AddFormFieldsToWebData(wds1_, keys1);

  AutofillKeys keys2;
  keys2.insert(AutofillKey("name1", "value2"));
  keys2.insert(AutofillKey("name2", "value3"));
  keys2.insert(AutofillKey("name3", "value4"));
  keys2.insert(AutofillKey("name4", "value4"));
  AddFormFieldsToWebData(wds2_, keys2);

  SetupSync();
  // Wait for client1 to get the new keys from client2.
  EXPECT_TRUE(client1()->AwaitSyncCycleCompletion("sync cycle"));

  AutofillKeys expected_keys;
  expected_keys.insert(AutofillKey("name1", "value1"));
  expected_keys.insert(AutofillKey("name1", "value2"));
  expected_keys.insert(AutofillKey("name2", "value3"));
  expected_keys.insert(AutofillKey("name3", "value4"));
  expected_keys.insert(AutofillKey("name4", "value4"));

  AutofillKeys wd1_keys;
  GetAllAutofillKeys(wds1_, &wd1_keys);
  EXPECT_EQ(expected_keys, wd1_keys);

  AutofillKeys wd2_keys;
  GetAllAutofillKeys(wds2_, &wd2_keys);
  EXPECT_EQ(expected_keys, wd2_keys);

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, Steady) {
  SetupHarness();
  SetupSync();

  // Client1 adds a key.
  AutofillKeys add_one_key;
  add_one_key.insert(AutofillKey("name1", "value1"));
  AddFormFieldsToWebData(wds1_, add_one_key);
  EXPECT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  AutofillKeys expected_keys;
  expected_keys.insert(AutofillKey("name1", "value1"));

  AutofillKeys keys;
  GetAllAutofillKeys(wds1_, &keys);
  EXPECT_EQ(expected_keys, keys);
  keys.clear();
  GetAllAutofillKeys(wds2_, &keys);
  EXPECT_EQ(expected_keys, keys);

  // Client2 adds a key.
  add_one_key.clear();
  add_one_key.insert(AutofillKey("name2", "value2"));
  AddFormFieldsToWebData(wds2_, add_one_key);
  EXPECT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  expected_keys.insert(AutofillKey("name2", "value2"));
  keys.clear();
  GetAllAutofillKeys(wds1_, &keys);
  EXPECT_EQ(expected_keys, keys);
  keys.clear();
  GetAllAutofillKeys(wds2_, &keys);
  EXPECT_EQ(expected_keys, keys);

  // Client1 adds a key with the same name.
  add_one_key.clear();
  add_one_key.insert(AutofillKey("name2", "value3"));
  AddFormFieldsToWebData(wds1_, add_one_key);
  EXPECT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  expected_keys.insert(AutofillKey("name2", "value3"));
  keys.clear();
  GetAllAutofillKeys(wds1_, &keys);
  EXPECT_EQ(expected_keys, keys);
  keys.clear();
  GetAllAutofillKeys(wds2_, &keys);
  EXPECT_EQ(expected_keys, keys);

  // Client2 removes a key.
  RemoveKeyFromWebData(wds2_, AutofillKey("name2", "value2"));
  EXPECT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  expected_keys.erase(AutofillKey("name2", "value2"));
  keys.clear();
  GetAllAutofillKeys(wds1_, &keys);
  EXPECT_EQ(expected_keys, keys);
  keys.clear();
  GetAllAutofillKeys(wds2_, &keys);
  EXPECT_EQ(expected_keys, keys);

  // Client1 removes the rest.
  RemoveKeyFromWebData(wds1_, AutofillKey("name1", "value1"));
  RemoveKeyFromWebData(wds1_, AutofillKey("name2", "value3"));
  EXPECT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  keys.clear();
  GetAllAutofillKeys(wds1_, &keys);
  EXPECT_EQ(0U, keys.size());
  keys.clear();
  GetAllAutofillKeys(wds2_, &keys);
  EXPECT_EQ(0U, keys.size());

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, ProfileClient1HasData) {
  SetupHarness();

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(MARION, expected_profiles[0]);
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(HOMER, expected_profiles[1]);
  AddProfile(pdm1_, *expected_profiles[0]);
  AddProfile(pdm1_, *expected_profiles[1]);

  SetupSync();

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm1_)));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm2_)));
  Cleanup();
  STLDeleteElements(&expected_profiles);
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest,
                       ProfileSameLabelOnDifferentClients) {
  SetupHarness();

  AutoFillProfiles profiles1;
  profiles1.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(HOMER, profiles1[0]);

  AutoFillProfiles profiles2;
  profiles2.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(MARION, profiles2[0]);
  profiles2[0]->set_label(ASCIIToUTF16("Shipping"));

  AddProfile(pdm1_, *profiles1[0]);
  AddProfile(pdm2_, *profiles2[0]);

  SetupSync();

  // Since client1 associates first, client2's "Shipping" profile will
  // be overwritten by the one stored in the cloud by profile1.
  EXPECT_TRUE(CompareAutoFillProfiles(profiles1,
                                      GetAllAutoFillProfiles(pdm1_)));
  EXPECT_TRUE(CompareAutoFillProfiles(profiles1,
                                      GetAllAutoFillProfiles(pdm2_)));
  Cleanup();
  STLDeleteElements(&profiles1);
  STLDeleteElements(&profiles2);
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest,
                       ProfileSameLabelOnClient1) {
  SetupHarness();

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(HOMER, expected_profiles[0]);
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(HOMER, expected_profiles[1]);

  AddProfile(pdm1_, *expected_profiles[0]);
  AddProfile(pdm1_, *expected_profiles[1]);

  SetupSync();

  // One of the duplicate profiles will have its label renamed to
  // "Shipping2".
  expected_profiles[0]->set_label(ASCIIToUTF16("Shipping2"));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm1_)));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm2_)));
  Cleanup();
  STLDeleteElements(&expected_profiles);
}


IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, ProfileSteady) {
  SetupHarness();
  SetupSync();

  // Client1 adds a profile.
  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(HOMER, expected_profiles[0]);
  AddProfile(pdm1_, *expected_profiles[0]);
  EXPECT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm1_)));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm2_)));

  // Client2 adds a profile.
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(MARION, expected_profiles[1]);
  AddProfile(pdm2_, *expected_profiles[1]);
  EXPECT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm1_)));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm2_)));

  // Client1 adds a conflicting profile.
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(MARION, expected_profiles[2]);
  AddProfile(pdm1_, *expected_profiles[2]);
  EXPECT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  // The conflicting profile's label will be made unique.
  expected_profiles[2]->set_label(ASCIIToUTF16("Billing2"));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm1_)));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm2_)));

  // Client2 removes a profile.
  delete expected_profiles.front();
  expected_profiles.erase(expected_profiles.begin());
  RemoveProfile(pdm2_, ASCIIToUTF16("Shipping"));
  EXPECT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm1_)));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm2_)));

  // Client1 updates a profile.
  expected_profiles[0]->SetInfo(AutoFillType(NAME_FIRST),
                                ASCIIToUTF16("Bart"));
  UpdateProfile(pdm1_,
                ASCIIToUTF16("Billing"),
                AutoFillType(NAME_FIRST),
                ASCIIToUTF16("Bart"));
  EXPECT_TRUE(client1()->AwaitMutualSyncCycleCompletion(client2()));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm1_)));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm2_)));

  // Client2 removes everything.
  STLDeleteElements(&expected_profiles);
  RemoveProfile(pdm2_, ASCIIToUTF16("Billing"));
  RemoveProfile(pdm2_, ASCIIToUTF16("Billing2"));
  EXPECT_TRUE(client2()->AwaitMutualSyncCycleCompletion(client1()));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm1_)));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
                                      GetAllAutoFillProfiles(pdm2_)));

  Cleanup();
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/preferences_manager.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/preferences/public/interfaces/preferences.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test implementation of prefs::mojom::PreferencesObserver which just tracks
// calls to OnPreferencesChanged.
class TestPreferencesObserver : public prefs::mojom::PreferencesObserver {
 public:
  TestPreferencesObserver(
      mojo::InterfaceRequest<prefs::mojom::PreferencesObserver> request)
      : on_preferences_changed_called_(false),
        binding_(this, std::move(request)) {}
  ~TestPreferencesObserver() override {}

  // Returns true is |key| was in the last set of preferences changed.
  bool KeyReceived(const std::string& key);

  // Clears the values set from the last OnPreferencesChanged.
  void Reset();

  bool on_preferences_changed_called() {
    return on_preferences_changed_called_;
  }

  const base::Value* on_preferences_changed_values() {
    return on_preferences_changed_values_.get();
  }

 private:
  // prefs::mojom::PreferencesObserver:
  void OnPreferencesChanged(
      std::unique_ptr<base::DictionaryValue> preferences) override {
    on_preferences_changed_called_ = true;
    on_preferences_changed_values_ = std::move(preferences);
  }

  bool on_preferences_changed_called_;
  std::unique_ptr<base::Value> on_preferences_changed_values_;

  mojo::Binding<PreferencesObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestPreferencesObserver);
};

bool TestPreferencesObserver::KeyReceived(const std::string& key) {
  base::DictionaryValue* dictionary = nullptr;
  on_preferences_changed_values_->GetAsDictionary(&dictionary);
  return dictionary->HasKey(key);
}

void TestPreferencesObserver::Reset() {
  on_preferences_changed_called_ = false;
  on_preferences_changed_values_.reset();
}

}  // namespace

namespace test {
class PreferencesManagerTest : public testing::Test {
 public:
  PreferencesManagerTest();
  ~PreferencesManagerTest() override;

  // Initializes the connection between |observer_| and |manager_|, subscribing
  // to changes to |preferences|.
  void InitObserver(const std::vector<std::string>& preferences);

  // Initializes a preference with |registry_| with a default |value|.
  void InitPreference(const std::string& key, int value);

  // Has |manager_| update the PrefStore with |preferences|.
  void SetPreferences(std::unique_ptr<base::DictionaryValue> preferences);

  PreferencesManager* manager() { return manager_.get(); }
  TestPreferencesObserver* observer() { return observer_.get(); }
  PrefChangeRegistrar* preferences_change_registrar() {
    return manager_->preferences_change_registrar_.get();
  }
  TestingProfile* profile() { return profile_; }
  user_prefs::PrefRegistrySyncable* registry() { return registry_; }
  PrefService* service() { return profile_->GetPrefs(); }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  // Sets up threads needed for |testing_profile_manager_|
  content::TestBrowserThreadBundle thread_bundle_;

  // Handles creation of profiles for testing.
  TestingProfileManager testing_profile_manager_;

  // Not owned
  TestingProfile* profile_;
  user_prefs::PrefRegistrySyncable* registry_;

  prefs::mojom::PreferencesObserverPtr proxy_;
  std::unique_ptr<TestPreferencesObserver> observer_;
  std::unique_ptr<PreferencesManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(PreferencesManagerTest);
};

PreferencesManagerTest::PreferencesManagerTest()
    : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

PreferencesManagerTest::~PreferencesManagerTest() {}

void PreferencesManagerTest::InitObserver(
    const std::vector<std::string>& preferences) {
  manager_->Subscribe(preferences);
  base::RunLoop().RunUntilIdle();
}

void PreferencesManagerTest::InitPreference(const std::string& key, int value) {
  registry_->RegisterIntegerPref(key, value);
  base::FundamentalValue fundamental_value(value);
  profile_->GetPrefs()->Set(key, fundamental_value);
}

void PreferencesManagerTest::SetPreferences(
    std::unique_ptr<base::DictionaryValue> preferences) {
  manager_->SetPreferences(std::move(preferences));
  base::RunLoop().RunUntilIdle();
}

void PreferencesManagerTest::SetUp() {
  ASSERT_TRUE(testing_profile_manager_.SetUp());

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> service(
      base::MakeUnique<sync_preferences::TestingPrefServiceSyncable>());
  registry_ = service->registry();
  chrome::RegisterUserProfilePrefs(registry_);

  const std::string kName = "navi";
  profile_ = testing_profile_manager_.CreateTestingProfile(
      kName, std::move(service), base::UTF8ToUTF16(kName), 0, std::string(),
      TestingProfile::TestingFactories());
  ASSERT_NE(nullptr, profile_->GetPrefs());

  observer_.reset(new TestPreferencesObserver(mojo::MakeRequest(&proxy_)));
  manager_ = base::MakeUnique<PreferencesManager>(std::move(proxy_), profile_);
  ASSERT_TRUE(manager_->preferences_change_registrar_->IsEmpty());
}

void PreferencesManagerTest::TearDown() {
  testing_profile_manager_.DeleteAllTestingProfiles();
}

// Tests that when the PrefService is empty that no subscriptions are made.
TEST_F(PreferencesManagerTest, EmptyService) {
  const std::string kKey = "hey";
  std::vector<std::string> preferences;
  preferences.push_back(kKey);
  InitObserver(preferences);
  EXPECT_FALSE(preferences_change_registrar()->IsObserved(kKey));
  EXPECT_FALSE(observer()->on_preferences_changed_called());
}

// Tests that when the PrefService has the desired key, that a subscription is
// setup and that the PreferenceObserver is notified.
TEST_F(PreferencesManagerTest, ServiceHasValues) {
  const std::string kKey = "hey";
  const int kValue = 42;
  InitPreference(kKey, kValue);

  std::vector<std::string> preferences;
  preferences.push_back(kKey);
  InitObserver(preferences);
  EXPECT_TRUE(preferences_change_registrar()->IsObserved(kKey));
  EXPECT_TRUE(observer()->on_preferences_changed_called());
  EXPECT_TRUE(observer()->KeyReceived(kKey));
}

// Tests that mulitple keys can be subscribed to.
TEST_F(PreferencesManagerTest, MultipleSubscriptions) {
  const std::string kKey1 = "hey";
  const int kValue1 = 42;
  InitPreference(kKey1, kValue1);

  const std::string kKey2 = "listen";
  const int kValue2 = 9001;
  InitPreference(kKey2, kValue2);

  std::vector<std::string> preferences;
  preferences.push_back(kKey1);
  preferences.push_back(kKey2);
  InitObserver(preferences);
  EXPECT_TRUE(preferences_change_registrar()->IsObserved(kKey1));
  EXPECT_TRUE(preferences_change_registrar()->IsObserved(kKey2));
  EXPECT_TRUE(observer()->KeyReceived(kKey1));
  EXPECT_TRUE(observer()->KeyReceived(kKey2));
}

// Tests that when all keys are not in the PrefService that subscriptions are
// set for the available key.
TEST_F(PreferencesManagerTest, PartialSubsriptionAvailable) {
  const std::string kKey1 = "hey";
  const int kValue1 = 42;
  InitPreference(kKey1, kValue1);

  const std::string kKey2 = "listen";
  std::vector<std::string> preferences;
  preferences.push_back(kKey1);
  preferences.push_back(kKey2);
  InitObserver(preferences);
  EXPECT_TRUE(preferences_change_registrar()->IsObserved(kKey1));
  EXPECT_FALSE(preferences_change_registrar()->IsObserved(kKey2));
  EXPECT_TRUE(observer()->KeyReceived(kKey1));
  EXPECT_FALSE(observer()->KeyReceived(kKey2));
}

// Tests that when a preference is changed that the PreferenceObserver is
// notified.
TEST_F(PreferencesManagerTest, PreferenceChanged) {
  const std::string kKey = "hey";
  const int kValue = 42;
  InitPreference(kKey, kValue);

  std::vector<std::string> preferences;
  preferences.push_back(kKey);
  InitObserver(preferences);
  observer()->Reset();

  const int kNewValue = 1337;
  service()->SetInteger(kKey, kNewValue);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer()->on_preferences_changed_called());
  const base::Value* values = observer()->on_preferences_changed_values();
  const base::DictionaryValue* dictionary = nullptr;
  values->GetAsDictionary(&dictionary);
  int result = 0;
  dictionary->GetInteger(kKey, &result);
  EXPECT_EQ(kNewValue, result);
}

// Tests that when a non subscribed preference is changed that the
// PreferenceObserver is not notified.
TEST_F(PreferencesManagerTest, UnrelatedPreferenceChanged) {
  const std::string kKey1 = "hey";
  const int kValue1 = 42;
  InitPreference(kKey1, kValue1);

  const std::string kKey2 = "listen";
  const int kValue2 = 9001;
  InitPreference(kKey2, kValue2);

  std::vector<std::string> preferences;
  preferences.push_back(kKey1);
  InitObserver(preferences);
  observer()->Reset();

  const int kNewValue = 1337;
  service()->SetInteger(kKey2, kNewValue);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(observer()->on_preferences_changed_called());
}

// Tests that when the PreferenceManager updates a preference that the
// PreferenceObserver is not notified.
TEST_F(PreferencesManagerTest, NoNotificationsForSelfChange) {
  const std::string kKey = "hey";
  const int kValue = 42;
  InitPreference(kKey, kValue);

  std::vector<std::string> preferences;
  preferences.push_back(kKey);
  InitObserver(preferences);
  observer()->Reset();

  const int kNewValue = 1337;
  std::unique_ptr<base::DictionaryValue> dictionary =
      base::MakeUnique<base::DictionaryValue>();
  dictionary->SetInteger(kKey, kNewValue);
  SetPreferences(std::move(dictionary));

  EXPECT_FALSE(observer()->on_preferences_changed_called());
  EXPECT_EQ(kNewValue, service()->GetInteger(kKey));
}

}  // namespace test

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/dummy_pref_store.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace {

class MockPolicyRefreshCallback {
 public:
  MockPolicyRefreshCallback() {}
  MOCK_METHOD1(DoCallback, void(const std::vector<std::string>));
};

}  // namespace

// Names of the preferences used in this test program.
namespace prefs {
const char kMissingPref[] = "this.pref.does_not_exist";
const char kRecommendedPref[] = "this.pref.recommended_value_only";
const char kSampleDict[] = "sample.dict";
const char kSampleList[] = "sample.list";
const char kDefaultPref[] = "default.pref";
}

// Potentially expected values of all preferences used in this test program.
namespace managed_platform_pref {
const std::string kHomepageValue = "http://www.topeka.com";
}

namespace device_management_pref {
const std::string kSearchProviderNameValue = "Chromium";
const char kHomepageValue[] = "http://www.wandering-around.org";
}

namespace extension_pref {
const char kCurrentThemeIDValue[] = "set by extension";
const char kHomepageValue[] = "http://www.chromium.org";
const std::string kSearchProviderNameValue = "AreYouFeelingALittleLucky";
}

namespace command_line_pref {
const char kApplicationLocaleValue[] = "hi-MOM";
const char kCurrentThemeIDValue[] = "zyxwvut";
const char kHomepageValue[] = "http://www.ferretcentral.org";
const std::string kSearchProviderNameValue = "AreYouFeelingPrettyLucky";
}

// The "user" namespace is defined globally in an ARM system header, so we need
// something different here.
namespace user_pref {
const int kStabilityLaunchCountValue = 31;
const bool kDeleteCacheValue = true;
const char kCurrentThemeIDValue[] = "abcdefg";
const char kHomepageValue[] = "http://www.google.com";
const char kApplicationLocaleValue[] = "is-WRONG";
const std::string kSearchProviderNameValue = "AreYouFeelingVeryLucky";
}

namespace recommended_pref {
const int kStabilityLaunchCountValue = 10;
const bool kRecommendedPrefValue = true;
}

namespace default_pref {
const int kDefaultValue = 7;
const char kHomepageValue[] = "default homepage";
const std::string kSearchProviderNameValue = "AreYouFeelingExtraLucky";
}

class PrefValueStoreTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create dummy user preferences.
    managed_platform_prefs_= CreateManagedPlatformPrefs();
    device_management_prefs_ = CreateDeviceManagementPrefs();
    extension_prefs_ = CreateExtensionPrefs();
    command_line_prefs_ = CreateCommandLinePrefs();
    user_prefs_ = CreateUserPrefs();
    recommended_prefs_ = CreateRecommendedPrefs();
    default_prefs_ = CreateDefaultPrefs();

    std::sort(expected_differing_paths_.begin(),
              expected_differing_paths_.end());

    // Create |DummyPrefStore|s.
    managed_platform_pref_store_ = new DummyPrefStore();
    managed_platform_pref_store_->set_prefs(managed_platform_prefs_);
    device_management_pref_store_ = new DummyPrefStore();
    device_management_pref_store_->set_prefs(device_management_prefs_);
    extension_pref_store_ = new DummyPrefStore();
    extension_pref_store_->set_prefs(extension_prefs_);
    command_line_pref_store_ = new DummyPrefStore();
    command_line_pref_store_->set_prefs(command_line_prefs_);
    user_pref_store_ = new DummyPrefStore();
    user_pref_store_->set_read_only(false);
    user_pref_store_->set_prefs(user_prefs_);
    recommended_pref_store_ = new DummyPrefStore();
    recommended_pref_store_->set_prefs(recommended_prefs_);
    default_pref_store_ = new DummyPrefStore();
    default_pref_store_->set_prefs(default_prefs_);

    // Create a new pref-value-store.
    pref_value_store_ = new TestingPrefService::TestingPrefValueStore(
        managed_platform_pref_store_,
        device_management_pref_store_,
        extension_pref_store_,
        command_line_pref_store_,
        user_pref_store_,
        recommended_pref_store_,
        default_pref_store_);

    // Register prefs with the PrefValueStore.
    pref_value_store_->RegisterPreferenceType(prefs::kApplicationLocale,
                                              Value::TYPE_STRING);
    pref_value_store_->RegisterPreferenceType(prefs::kCurrentThemeID,
                                              Value::TYPE_STRING);
    pref_value_store_->RegisterPreferenceType(
        prefs::kDefaultSearchProviderName,
        Value::TYPE_STRING);
    pref_value_store_->RegisterPreferenceType(prefs::kDeleteCache,
                                              Value::TYPE_BOOLEAN);
    pref_value_store_->RegisterPreferenceType(prefs::kHomePage,
                                              Value::TYPE_STRING);
    pref_value_store_->RegisterPreferenceType(prefs::kStabilityLaunchCount,
                                              Value::TYPE_INTEGER);
    pref_value_store_->RegisterPreferenceType(prefs::kRecommendedPref,
                                              Value::TYPE_BOOLEAN);
    pref_value_store_->RegisterPreferenceType(prefs::kSampleDict,
                                              Value::TYPE_DICTIONARY);
    pref_value_store_->RegisterPreferenceType(prefs::kSampleList,
                                              Value::TYPE_LIST);
    pref_value_store_->RegisterPreferenceType(prefs::kDefaultPref,
                                              Value::TYPE_INTEGER);
    pref_value_store_->RegisterPreferenceType(prefs::kProxyAutoDetect,
                                              Value::TYPE_BOOLEAN);

    ui_thread_.reset(new BrowserThread(BrowserThread::UI, &loop_));
    file_thread_.reset(new BrowserThread(BrowserThread::FILE, &loop_));
  }

  // Creates a new dictionary and stores some sample user preferences
  // in it.
  DictionaryValue* CreateUserPrefs() {
    DictionaryValue* user_prefs = new DictionaryValue();
    user_prefs->SetBoolean(prefs::kDeleteCache, user_pref::kDeleteCacheValue);
    user_prefs->SetInteger(prefs::kStabilityLaunchCount,
                           user_pref::kStabilityLaunchCountValue);
    user_prefs->SetString(prefs::kCurrentThemeID,
        user_pref::kCurrentThemeIDValue);
    user_prefs->SetString(prefs::kApplicationLocale,
        user_pref::kApplicationLocaleValue);
    user_prefs->SetString(prefs::kDefaultSearchProviderName,
        user_pref::kSearchProviderNameValue);
    user_prefs->SetString(prefs::kHomePage, user_pref::kHomepageValue);
    return user_prefs;
  }

  DictionaryValue* CreateManagedPlatformPrefs() {
    DictionaryValue* managed_platform_prefs = new DictionaryValue();
    managed_platform_prefs->SetString(
        prefs::kHomePage,
        managed_platform_pref::kHomepageValue);
    expected_differing_paths_.push_back(prefs::kHomePage);
    return managed_platform_prefs;
  }

  DictionaryValue* CreateDeviceManagementPrefs() {
    DictionaryValue* device_management_prefs = new DictionaryValue();
    device_management_prefs->SetString(
        prefs::kDefaultSearchProviderName,
        device_management_pref::kSearchProviderNameValue);
    expected_differing_paths_.push_back("default_search_provider");
    expected_differing_paths_.push_back(prefs::kDefaultSearchProviderName);
    device_management_prefs->SetString(prefs::kHomePage,
                                       device_management_pref::kHomepageValue);
    return device_management_prefs;
  }

  DictionaryValue* CreateExtensionPrefs() {
    DictionaryValue* extension_prefs = new DictionaryValue();
    extension_prefs->SetString(prefs::kCurrentThemeID,
        extension_pref::kCurrentThemeIDValue);
    extension_prefs->SetString(prefs::kHomePage,
        extension_pref::kHomepageValue);
    extension_prefs->SetString(prefs::kDefaultSearchProviderName,
                               extension_pref::kSearchProviderNameValue);
    return extension_prefs;
  }

  DictionaryValue* CreateCommandLinePrefs() {
    DictionaryValue* command_line_prefs = new DictionaryValue();
    command_line_prefs->SetString(prefs::kCurrentThemeID,
        command_line_pref::kCurrentThemeIDValue);
    command_line_prefs->SetString(prefs::kApplicationLocale,
        command_line_pref::kApplicationLocaleValue);
    command_line_prefs->SetString(prefs::kHomePage,
        command_line_pref::kHomepageValue);
    command_line_prefs->SetString(
        prefs::kDefaultSearchProviderName,
        command_line_pref::kSearchProviderNameValue);
    return command_line_prefs;
  }

  DictionaryValue* CreateRecommendedPrefs() {
    DictionaryValue* recommended_prefs = new DictionaryValue();
    recommended_prefs->SetInteger(prefs::kStabilityLaunchCount,
        recommended_pref::kStabilityLaunchCountValue);
    recommended_prefs->SetBoolean(
        prefs::kRecommendedPref,
        recommended_pref::kRecommendedPrefValue);

    // Expected differing paths must be added in lexicographic order
    // to work properly
    expected_differing_paths_.push_back("this");
    expected_differing_paths_.push_back("this.pref");
    expected_differing_paths_.push_back(prefs::kRecommendedPref);
    expected_differing_paths_.push_back("user_experience_metrics");
    expected_differing_paths_.push_back("user_experience_metrics.stability");
    expected_differing_paths_.push_back(prefs::kStabilityLaunchCount);
    return recommended_prefs;
  }

  DictionaryValue* CreateDefaultPrefs() {
    DictionaryValue* default_prefs = new DictionaryValue();
    default_prefs->SetInteger(prefs::kDefaultPref, default_pref::kDefaultValue);
    return default_prefs;
  }

  DictionaryValue* CreateSampleDictValue() {
    DictionaryValue* sample_dict = new DictionaryValue();
    sample_dict->SetBoolean("issample", true);
    sample_dict->SetInteger("value", 4);
    sample_dict->SetString("descr", "Sample Test Dictionary");
    return sample_dict;
  }

  ListValue* CreateSampleListValue() {
    ListValue* sample_list = new ListValue();
    sample_list->Set(0, Value::CreateIntegerValue(0));
    sample_list->Set(1, Value::CreateIntegerValue(1));
    sample_list->Set(2, Value::CreateIntegerValue(2));
    sample_list->Set(3, Value::CreateIntegerValue(3));
    return sample_list;
  }

  virtual void TearDown() {
    loop_.RunAllPending();
  }

  MessageLoop loop_;

  scoped_refptr<TestingPrefService::TestingPrefValueStore> pref_value_store_;

  // |PrefStore|s are owned by the |PrefValueStore|.
  DummyPrefStore* managed_platform_pref_store_;
  DummyPrefStore* device_management_pref_store_;
  DummyPrefStore* extension_pref_store_;
  DummyPrefStore* command_line_pref_store_;
  DummyPrefStore* user_pref_store_;
  DummyPrefStore* recommended_pref_store_;
  DummyPrefStore* default_pref_store_;

  // A vector of the preferences paths in the managed and recommended
  // PrefStores that are set at the beginning of a test. Can be modified
  // by the test to track changes that it makes to the preferences
  // stored in the managed and recommended PrefStores.
  std::vector<std::string> expected_differing_paths_;

  // Preferences are owned by the individual |DummyPrefStores|.
  DictionaryValue* managed_platform_prefs_;
  DictionaryValue* device_management_prefs_;
  DictionaryValue* extension_prefs_;
  DictionaryValue* command_line_prefs_;
  DictionaryValue* user_prefs_;
  DictionaryValue* recommended_prefs_;
  DictionaryValue* default_prefs_;

 private:
  scoped_ptr<BrowserThread> ui_thread_;
  scoped_ptr<BrowserThread> file_thread_;
};

TEST_F(PrefValueStoreTest, IsReadOnly) {
  managed_platform_pref_store_->set_read_only(true);
  extension_pref_store_->set_read_only(true);
  command_line_pref_store_->set_read_only(true);
  user_pref_store_->set_read_only(true);
  recommended_pref_store_->set_read_only(true);
  default_pref_store_->set_read_only(true);
  EXPECT_TRUE(pref_value_store_->ReadOnly());

  user_pref_store_->set_read_only(false);
  EXPECT_FALSE(pref_value_store_->ReadOnly());
}

TEST_F(PrefValueStoreTest, GetValue) {
  Value* value;

  // Test getting a managed platform value overwriting a user-defined and
  // extension-defined value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kHomePage, &value));
  std::string actual_str_value;
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(managed_platform_pref::kHomepageValue, actual_str_value);

  // Test getting a managed platform value overwriting a user-defined value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kDefaultSearchProviderName,
                                          &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(device_management_pref::kSearchProviderNameValue,
            actual_str_value);

  // Test getting an extension value overwriting a user-defined and
  // command-line-defined value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kCurrentThemeID, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(extension_pref::kCurrentThemeIDValue, actual_str_value);

  // Test getting a command-line value overwriting a user-defined value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kApplicationLocale, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(command_line_pref::kApplicationLocaleValue, actual_str_value);

  // Test getting a user-set value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kDeleteCache, &value));
  bool actual_bool_value = false;
  EXPECT_TRUE(value->GetAsBoolean(&actual_bool_value));
  EXPECT_EQ(user_pref::kDeleteCacheValue, actual_bool_value);

  // Test getting a user set value overwriting a recommended value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kStabilityLaunchCount,
                                          &value));
  int actual_int_value = -1;
  EXPECT_TRUE(value->GetAsInteger(&actual_int_value));
  EXPECT_EQ(user_pref::kStabilityLaunchCountValue, actual_int_value);

  // Test getting a recommended value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kRecommendedPref, &value));
  actual_bool_value = false;
  EXPECT_TRUE(value->GetAsBoolean(&actual_bool_value));
  EXPECT_EQ(recommended_pref::kRecommendedPrefValue, actual_bool_value);

  // Test getting a default value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kDefaultPref, &value));
  actual_int_value = -1;
  EXPECT_TRUE(value->GetAsInteger(&actual_int_value));
  EXPECT_EQ(default_pref::kDefaultValue, actual_int_value);

  // Test getting a preference value that the |PrefValueStore|
  // does not contain.
  FundamentalValue tmp_dummy_value(true);
  Value* v_null = &tmp_dummy_value;
  ASSERT_FALSE(pref_value_store_->GetValue(prefs::kMissingPref, &v_null));
  ASSERT_TRUE(v_null == NULL);
}

// Make sure that if a preference changes type, so the wrong type is stored in
// the user pref file, it uses the correct fallback value instead.
TEST_F(PrefValueStoreTest, GetValueChangedType) {
  // Check falling back to a recommended value.
  user_pref_store_->prefs()->SetString(prefs::kStabilityLaunchCount,
                                       "not an integer");
  Value* value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kStabilityLaunchCount,
                                          &value));
  ASSERT_TRUE(value != NULL);
  ASSERT_EQ(Value::TYPE_INTEGER, value->GetType());
  int actual_int_value = -1;
  EXPECT_TRUE(value->GetAsInteger(&actual_int_value));
  EXPECT_EQ(recommended_pref::kStabilityLaunchCountValue, actual_int_value);

  // Check falling back multiple times, to a default string.
  managed_platform_pref_store_->prefs()->SetInteger(prefs::kHomePage, 1);
  device_management_pref_store_->prefs()->SetInteger(prefs::kHomePage, 1);
  extension_pref_store_->prefs()->SetInteger(prefs::kHomePage, 1);
  command_line_pref_store_->prefs()->SetInteger(prefs::kHomePage, 1);
  user_pref_store_->prefs()->SetInteger(prefs::kHomePage, 1);
  recommended_pref_store_->prefs()->SetInteger(prefs::kHomePage, 1);
  default_pref_store_->prefs()->SetString(prefs::kHomePage,
                                          default_pref::kHomepageValue);

  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kHomePage, &value));
  ASSERT_TRUE(value != NULL);
  ASSERT_EQ(Value::TYPE_STRING, value->GetType());
  std::string actual_str_value;
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(default_pref::kHomepageValue, actual_str_value);
}

TEST_F(PrefValueStoreTest, HasPrefPath) {
  // Managed Platform preference
  EXPECT_TRUE(pref_value_store_->HasPrefPath(prefs::kHomePage));
  // Device management preference
  EXPECT_TRUE(pref_value_store_->HasPrefPath(
      prefs::kDefaultSearchProviderName));
  // Extension preference
  EXPECT_TRUE(pref_value_store_->HasPrefPath(prefs::kCurrentThemeID));
  // User preference
  EXPECT_TRUE(pref_value_store_->HasPrefPath(prefs::kDeleteCache));
  // Recommended preference
  EXPECT_TRUE(pref_value_store_->HasPrefPath(prefs::kRecommendedPref));
  // Default preference
  EXPECT_FALSE(pref_value_store_->HasPrefPath(prefs::kDefaultPref));
  // Unknown preference
  EXPECT_FALSE(pref_value_store_->HasPrefPath(prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefHasChanged) {
  // Setup.
  const char managed_platform_pref_path[] = "managed_platform_pref";
  pref_value_store_->RegisterPreferenceType(managed_platform_pref_path,
                                            Value::TYPE_STRING);
  managed_platform_pref_store_->prefs()->SetString(managed_platform_pref_path,
                                                   "managed value");
  const char user_pref_path[] = "user_pref";
  pref_value_store_->RegisterPreferenceType(user_pref_path, Value::TYPE_STRING);
  user_pref_store_->prefs()->SetString(user_pref_path, "user value");
  const char default_pref_path[] = "default_pref";
  pref_value_store_->RegisterPreferenceType(default_pref_path,
                                            Value::TYPE_STRING);
  default_pref_store_->prefs()->SetString(default_pref_path, "default value");

  // Check pref controlled by highest-priority store.
  EXPECT_TRUE(pref_value_store_->PrefHasChanged(managed_platform_pref_path,
      static_cast<PrefNotifier::PrefStoreType>(0)));
  EXPECT_FALSE(pref_value_store_->PrefHasChanged(managed_platform_pref_path,
      PrefNotifier::USER_STORE));

  // Check pref controlled by user store.
  EXPECT_TRUE(pref_value_store_->PrefHasChanged(user_pref_path,
      static_cast<PrefNotifier::PrefStoreType>(0)));
  EXPECT_TRUE(pref_value_store_->PrefHasChanged(user_pref_path,
      PrefNotifier::USER_STORE));
  EXPECT_FALSE(pref_value_store_->PrefHasChanged(user_pref_path,
      PrefNotifier::PREF_STORE_TYPE_MAX));

  // Check pref controlled by default-pref store.
  EXPECT_TRUE(pref_value_store_->PrefHasChanged(default_pref_path,
      PrefNotifier::USER_STORE));
  EXPECT_TRUE(pref_value_store_->PrefHasChanged(default_pref_path,
      PrefNotifier::DEFAULT_STORE));
}

TEST_F(PrefValueStoreTest, ReadPrefs) {
  pref_value_store_->ReadPrefs();
  // The ReadPrefs method of the |DummyPrefStore| deletes the |pref_store|s
  // internal dictionary and creates a new empty dictionary. Hence this
  // dictionary does not contain any of the preloaded preferences.
  // This shows that the ReadPrefs method of the |DummyPrefStore| was called.
  EXPECT_FALSE(pref_value_store_->HasPrefPath(prefs::kDeleteCache));
}

TEST_F(PrefValueStoreTest, WritePrefs) {
  user_pref_store_->set_prefs_written(false);
  pref_value_store_->WritePrefs();
  ASSERT_TRUE(user_pref_store_->get_prefs_written());
}

TEST_F(PrefValueStoreTest, SetUserPrefValue) {
  Value* new_value = NULL;
  Value* actual_value = NULL;

  // Test that managed platform values can not be set.
  ASSERT_TRUE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kHomePage));
  // The Ownership is tranfered to |PrefValueStore|.
  new_value = Value::CreateStringValue("http://www.youtube.com");
  pref_value_store_->SetUserPrefValue(prefs::kHomePage, new_value);

  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kHomePage, &actual_value));
  std::string value_str;
  actual_value->GetAsString(&value_str);
  ASSERT_EQ(managed_platform_pref::kHomepageValue, value_str);

  // User preferences values can be set
  ASSERT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kStabilityLaunchCount));
  actual_value = NULL;
  pref_value_store_->GetValue(prefs::kStabilityLaunchCount, &actual_value);
  int int_value;
  EXPECT_TRUE(actual_value->GetAsInteger(&int_value));
  EXPECT_EQ(user_pref::kStabilityLaunchCountValue, int_value);

  new_value = Value::CreateIntegerValue(1);
  pref_value_store_->SetUserPrefValue(prefs::kStabilityLaunchCount, new_value);
  actual_value = NULL;
  pref_value_store_->GetValue(prefs::kStabilityLaunchCount, &actual_value);
  EXPECT_TRUE(new_value->Equals(actual_value));

  // Set and Get |DictionaryValue|
  DictionaryValue* expected_dict_value = CreateSampleDictValue();
  pref_value_store_->SetUserPrefValue(prefs::kSampleDict, expected_dict_value);

  actual_value = NULL;
  std::string key(prefs::kSampleDict);
  pref_value_store_->GetValue(key, &actual_value);

  ASSERT_EQ(expected_dict_value, actual_value);
  ASSERT_TRUE(expected_dict_value->Equals(actual_value));

  // Set and Get a |ListValue|
  ListValue* expected_list_value = CreateSampleListValue();
  pref_value_store_->SetUserPrefValue(prefs::kSampleList, expected_list_value);

  actual_value = NULL;
  key = prefs::kSampleList;
  pref_value_store_->GetValue(key, &actual_value);

  ASSERT_EQ(expected_list_value, actual_value);
  ASSERT_TRUE(expected_list_value->Equals(actual_value));
}

TEST_F(PrefValueStoreTest, PrefValueInManagedPlatformStore) {
  // Test a managed platform preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kHomePage));
  EXPECT_TRUE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kHomePage));

  // Test a device management preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(
      prefs::kDefaultSearchProviderName));
  EXPECT_TRUE(pref_value_store_->PrefValueInDeviceManagementStore(
      prefs::kDefaultSearchProviderName));

  // Test an extension preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kCurrentThemeID));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kCurrentThemeID));

  // Test a command-line preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kApplicationLocale));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kApplicationLocale));

  // Test a user preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kStabilityLaunchCount));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kStabilityLaunchCount));

  // Test a preference from the recommended pref store.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kRecommendedPref));

  // Test a preference from the default pref store.
  ASSERT_FALSE(pref_value_store_->HasPrefPath(prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kDefaultPref));

  // Test a preference for which the PrefValueStore does not contain a value.
  ASSERT_FALSE(pref_value_store_->HasPrefPath(prefs::kMissingPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueInExtensionStore) {
  // Test a managed platform preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kHomePage));
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(prefs::kHomePage));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kHomePage));

  // Test a device management preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(
      prefs::kDefaultSearchProviderName));
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kDefaultSearchProviderName));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kDefaultSearchProviderName));

  // Test an extension preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kCurrentThemeID));
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kCurrentThemeID));
  EXPECT_TRUE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kCurrentThemeID));

  // Test a command-line preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kApplicationLocale));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kApplicationLocale));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kApplicationLocale));

  // Test a user preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kStabilityLaunchCount));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kStabilityLaunchCount));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kStabilityLaunchCount));

  // Test a preference from the recommended pref store.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kRecommendedPref));

  // Test a preference from the default pref store.
  ASSERT_FALSE(pref_value_store_->HasPrefPath(prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kDefaultPref));

  // Test a preference for which the PrefValueStore does not contain a value.
  ASSERT_FALSE(pref_value_store_->HasPrefPath(prefs::kMissingPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kMissingPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, DetectProxyConfigurationConflict) {
  // There should be no conflicting proxy prefs in the default
  // preference stores created for the test.
  ASSERT_FALSE(pref_value_store_->HasPolicyConflictingUserProxySettings());

  // Create conflicting proxy settings in the managed and command-line
  // preference stores.
  command_line_prefs_->SetBoolean(prefs::kProxyAutoDetect, false);
  managed_platform_prefs_->SetBoolean(prefs::kProxyAutoDetect, true);
  ASSERT_TRUE(pref_value_store_->HasPolicyConflictingUserProxySettings());
}

TEST_F(PrefValueStoreTest, PrefValueInUserStore) {
  // Test a managed platform preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kHomePage));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(prefs::kHomePage));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(prefs::kHomePage));

  // Test a device management preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(
      prefs::kDefaultSearchProviderName));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kDefaultSearchProviderName));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kDefaultSearchProviderName));

  // Test an extension preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kCurrentThemeID));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kCurrentThemeID));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kCurrentThemeID));

  // Test a command-line preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kApplicationLocale));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kApplicationLocale));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kApplicationLocale));

  // Test a user preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kStabilityLaunchCount));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kStabilityLaunchCount));
  EXPECT_TRUE(pref_value_store_->PrefValueFromUserStore(
      prefs::kStabilityLaunchCount));

  // Test a preference from the recommended pref store.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kRecommendedPref));

  // Test a preference from the default pref store.
  ASSERT_FALSE(pref_value_store_->HasPrefPath(prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(prefs::kDefaultPref));

  // Test a preference for which the PrefValueStore does not contain a value.
  ASSERT_FALSE(pref_value_store_->HasPrefPath(prefs::kMissingPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(prefs::kMissingPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueFromDefaultStore) {
  // Test a managed platform preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kHomePage));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(prefs::kHomePage));

  // Test a device management preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(
      prefs::kDefaultSearchProviderName));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kDefaultSearchProviderName));

  // Test an extension preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kCurrentThemeID));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kCurrentThemeID));

  // Test a command-line preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kApplicationLocale));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kApplicationLocale));

  // Test a user preference.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kStabilityLaunchCount));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kStabilityLaunchCount));

  // Test a preference from the recommended pref store.
  ASSERT_TRUE(pref_value_store_->HasPrefPath(prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kRecommendedPref));

  // Test a preference from the default pref store.
  ASSERT_FALSE(pref_value_store_->HasPrefPath(prefs::kDefaultPref));
  EXPECT_TRUE(
      pref_value_store_->PrefValueFromDefaultStore(prefs::kDefaultPref));

  // Test a preference for which the PrefValueStore does not contain a value.
  ASSERT_FALSE(pref_value_store_->HasPrefPath(prefs::kMissingPref));
  EXPECT_FALSE(
      pref_value_store_->PrefValueFromDefaultStore(prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, TestPolicyRefresh) {
  // pref_value_store_ is initialized by PrefValueStoreTest to have values in
  // the managed platform, device management and recommended stores. By
  // replacing them with dummy stores, all of the paths of the prefs originally
  // in the managed platform, device management and recommended stores should
  // change.
  MockPolicyRefreshCallback callback;
  EXPECT_CALL(callback, DoCallback(_)).Times(0);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          pref_value_store_.get(),
          &PrefValueStore::RefreshPolicyPrefs,
          NewCallback(&callback,
                      &MockPolicyRefreshCallback::DoCallback)));
  Mock::VerifyAndClearExpectations(&callback);
  EXPECT_CALL(callback, DoCallback(expected_differing_paths_)).Times(1);
  loop_.RunAllPending();
}

TEST_F(PrefValueStoreTest, TestRefreshPolicyPrefsCompletion) {
  // Test changed preferences in the managed platform store and removed
  // preferences in the recommended store. In addition to "homepage", the other
  // prefs that are set by default in the test class are removed by the
  // DummyStore.
  scoped_ptr<DummyPrefStore> new_managed_platform_store(new DummyPrefStore());
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("homepage", "some other changed homepage");
  new_managed_platform_store->set_prefs(dict);
  MockPolicyRefreshCallback callback;
  EXPECT_CALL(callback, DoCallback(expected_differing_paths_)).Times(1);
  pref_value_store_->RefreshPolicyPrefsCompletion(
      new_managed_platform_store.release(),
      new DummyPrefStore(),
      new DummyPrefStore(),
      NewCallback(&callback,
                  &MockPolicyRefreshCallback::DoCallback));

  // Test properties that have been removed from the managed platform store.
  // Homepage is still set in managed prefs.
  expected_differing_paths_.clear();
  expected_differing_paths_.push_back(std::string("homepage"));
  MockPolicyRefreshCallback callback2;
  EXPECT_CALL(callback2, DoCallback(expected_differing_paths_)).Times(1);
  pref_value_store_->RefreshPolicyPrefsCompletion(
      new DummyPrefStore(),
      new DummyPrefStore(),
      new DummyPrefStore(),
      NewCallback(&callback2,
                  &MockPolicyRefreshCallback::DoCallback));

  // Test properties that are added to the device management store.
  expected_differing_paths_.clear();
  expected_differing_paths_.push_back(std::string("homepage"));
  scoped_ptr<DummyPrefStore> new_device_management_store(
      new DummyPrefStore());
  dict = new DictionaryValue();
  dict->SetString("homepage", "some other changed homepage");
  new_device_management_store->set_prefs(dict);
  MockPolicyRefreshCallback callback3;
  EXPECT_CALL(callback3, DoCallback(expected_differing_paths_)).Times(1);
  pref_value_store_->RefreshPolicyPrefsCompletion(
      new DummyPrefStore(),
      new_device_management_store.release(),
      new DummyPrefStore(),
      NewCallback(&callback3,
                  &MockPolicyRefreshCallback::DoCallback));

  // Test properties that are added to the recommended store.
  scoped_ptr<DummyPrefStore>  new_recommended_store(new DummyPrefStore());
  dict = new DictionaryValue();
  dict->SetString("homepage", "some other changed homepage 2");
  new_recommended_store->set_prefs(dict);
  expected_differing_paths_.clear();
  expected_differing_paths_.push_back(std::string("homepage"));
  MockPolicyRefreshCallback callback4;
  EXPECT_CALL(callback4, DoCallback(expected_differing_paths_)).Times(1);
  pref_value_store_->RefreshPolicyPrefsCompletion(
      new DummyPrefStore(),
      new DummyPrefStore(),
      new_recommended_store.release(),
      NewCallback(&callback4,
                  &MockPolicyRefreshCallback::DoCallback));

  // Test adding a multi-key path.
  new_managed_platform_store.reset(new DummyPrefStore());
  dict = new DictionaryValue();
  dict->SetString("segment1.segment2", "value");
  new_managed_platform_store->set_prefs(dict);
  expected_differing_paths_.clear();
  expected_differing_paths_.push_back(std::string("homepage"));
  expected_differing_paths_.push_back(std::string("segment1"));
  expected_differing_paths_.push_back(std::string("segment1.segment2"));
  MockPolicyRefreshCallback callback5;
  EXPECT_CALL(callback5, DoCallback(expected_differing_paths_)).Times(1);
  pref_value_store_->RefreshPolicyPrefsCompletion(
      new_managed_platform_store.release(),
      new DummyPrefStore(),
      new DummyPrefStore(),
      NewCallback(&callback5,
                  &MockPolicyRefreshCallback::DoCallback));
}

TEST_F(PrefValueStoreTest, TestConcurrentPolicyRefresh) {
  MockPolicyRefreshCallback callback1;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          pref_value_store_.get(),
          &PrefValueStore::RefreshPolicyPrefs,
          NewCallback(&callback1,
                      &MockPolicyRefreshCallback::DoCallback)));
  EXPECT_CALL(callback1, DoCallback(_)).Times(0);

  MockPolicyRefreshCallback callback2;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          pref_value_store_.get(),
          &PrefValueStore::RefreshPolicyPrefs,
          NewCallback(&callback2,
                      &MockPolicyRefreshCallback::DoCallback)));
  EXPECT_CALL(callback2, DoCallback(_)).Times(0);

  MockPolicyRefreshCallback callback3;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          pref_value_store_.get(),
          &PrefValueStore::RefreshPolicyPrefs,
          NewCallback(&callback3,
                      &MockPolicyRefreshCallback::DoCallback)));
  EXPECT_CALL(callback3, DoCallback(_)).Times(0);
  Mock::VerifyAndClearExpectations(&callback1);
  Mock::VerifyAndClearExpectations(&callback2);
  Mock::VerifyAndClearExpectations(&callback3);

  EXPECT_CALL(callback1, DoCallback(expected_differing_paths_)).Times(1);
  std::vector<std::string> no_differing_paths;
  EXPECT_CALL(callback2, DoCallback(no_differing_paths)).Times(1);
  EXPECT_CALL(callback3, DoCallback(no_differing_paths)).Times(1);
  loop_.RunAllPending();
}

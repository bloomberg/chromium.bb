// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/dummy_configuration_policy_provider.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Mock;
using testing::Invoke;

namespace {

// Allows to capture pref notifications through gmock.
class MockPrefNotifier : public PrefNotifier {
 public:
  MOCK_METHOD1(OnPreferenceChanged, void(const std::string&));
  MOCK_METHOD0(OnInitializationCompleted, void());
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
    // Create TestingPrefStores.
    CreateManagedPlatformPrefs();
    CreateDeviceManagementPrefs();
    CreateExtensionPrefs();
    CreateCommandLinePrefs();
    CreateUserPrefs();
    CreateRecommendedPrefs();
    CreateDefaultPrefs();

    // Create a fresh PrefValueStore.
    pref_value_store_.reset(new PrefValueStore(
        managed_platform_pref_store_,
        device_management_pref_store_,
        extension_pref_store_,
        command_line_pref_store_,
        user_pref_store_,
        recommended_pref_store_,
        default_pref_store_,
        &pref_notifier_));
  }

  // Creates a new dictionary and stores some sample user preferences
  // in it.
  void CreateUserPrefs() {
    user_pref_store_ = new TestingPrefStore;
    user_pref_store_->SetBoolean(prefs::kDeleteCache,
        user_pref::kDeleteCacheValue);
    user_pref_store_->SetInteger(prefs::kStabilityLaunchCount,
        user_pref::kStabilityLaunchCountValue);
    user_pref_store_->SetString(prefs::kCurrentThemeID,
        user_pref::kCurrentThemeIDValue);
    user_pref_store_->SetString(prefs::kApplicationLocale,
        user_pref::kApplicationLocaleValue);
    user_pref_store_->SetString(prefs::kDefaultSearchProviderName,
        user_pref::kSearchProviderNameValue);
    user_pref_store_->SetString(prefs::kHomePage,
        user_pref::kHomepageValue);
  }

  void CreateManagedPlatformPrefs() {
    managed_platform_pref_store_ = new TestingPrefStore;
    managed_platform_pref_store_->SetString(prefs::kHomePage,
        managed_platform_pref::kHomepageValue);
  }

  void CreateDeviceManagementPrefs() {
    device_management_pref_store_ = new TestingPrefStore;
    device_management_pref_store_->SetString(prefs::kDefaultSearchProviderName,
        device_management_pref::kSearchProviderNameValue);
    device_management_pref_store_->SetString(prefs::kHomePage,
        device_management_pref::kHomepageValue);
  }

  void CreateExtensionPrefs() {
    extension_pref_store_ = new TestingPrefStore;
    extension_pref_store_->SetString(prefs::kCurrentThemeID,
        extension_pref::kCurrentThemeIDValue);
    extension_pref_store_->SetString(prefs::kHomePage,
        extension_pref::kHomepageValue);
    extension_pref_store_->SetString(prefs::kDefaultSearchProviderName,
        extension_pref::kSearchProviderNameValue);
  }

  void CreateCommandLinePrefs() {
    command_line_pref_store_ = new TestingPrefStore;
    command_line_pref_store_->SetString(prefs::kCurrentThemeID,
        command_line_pref::kCurrentThemeIDValue);
    command_line_pref_store_->SetString(prefs::kApplicationLocale,
        command_line_pref::kApplicationLocaleValue);
    command_line_pref_store_->SetString(prefs::kHomePage,
        command_line_pref::kHomepageValue);
    command_line_pref_store_->SetString(prefs::kDefaultSearchProviderName,
        command_line_pref::kSearchProviderNameValue);
  }

  void CreateRecommendedPrefs() {
    recommended_pref_store_ = new TestingPrefStore;
    recommended_pref_store_->SetInteger(prefs::kStabilityLaunchCount,
        recommended_pref::kStabilityLaunchCountValue);
    recommended_pref_store_->SetBoolean(prefs::kRecommendedPref,
        recommended_pref::kRecommendedPrefValue);
  }

  void CreateDefaultPrefs() {
    default_pref_store_ = new TestingPrefStore;
    default_pref_store_->SetInteger(prefs::kDefaultPref,
                                    default_pref::kDefaultValue);
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

  MockPrefNotifier pref_notifier_;
  scoped_ptr<PrefValueStore> pref_value_store_;

  scoped_refptr<TestingPrefStore> managed_platform_pref_store_;
  scoped_refptr<TestingPrefStore> device_management_pref_store_;
  scoped_refptr<TestingPrefStore> extension_pref_store_;
  scoped_refptr<TestingPrefStore> command_line_pref_store_;
  scoped_refptr<TestingPrefStore> user_pref_store_;
  scoped_refptr<TestingPrefStore> recommended_pref_store_;
  scoped_refptr<TestingPrefStore> default_pref_store_;
};

TEST_F(PrefValueStoreTest, GetValue) {
  Value* value;

  // Test getting a managed platform value overwriting a user-defined and
  // extension-defined value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kHomePage, Value::TYPE_STRING,
                                          &value));
  std::string actual_str_value;
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(managed_platform_pref::kHomepageValue, actual_str_value);

  // Test getting a managed platform value overwriting a user-defined value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kDefaultSearchProviderName,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(device_management_pref::kSearchProviderNameValue,
            actual_str_value);

  // Test getting an extension value overwriting a user-defined and
  // command-line-defined value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kCurrentThemeID,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(extension_pref::kCurrentThemeIDValue, actual_str_value);

  // Test getting a command-line value overwriting a user-defined value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kApplicationLocale,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(command_line_pref::kApplicationLocaleValue, actual_str_value);

  // Test getting a user-set value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kDeleteCache,
                                          Value::TYPE_BOOLEAN, &value));
  bool actual_bool_value = false;
  EXPECT_TRUE(value->GetAsBoolean(&actual_bool_value));
  EXPECT_EQ(user_pref::kDeleteCacheValue, actual_bool_value);

  // Test getting a user set value overwriting a recommended value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kStabilityLaunchCount,
                                          Value::TYPE_INTEGER, &value));
  int actual_int_value = -1;
  EXPECT_TRUE(value->GetAsInteger(&actual_int_value));
  EXPECT_EQ(user_pref::kStabilityLaunchCountValue, actual_int_value);

  // Test getting a recommended value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kRecommendedPref,
                                          Value::TYPE_BOOLEAN, &value));
  actual_bool_value = false;
  EXPECT_TRUE(value->GetAsBoolean(&actual_bool_value));
  EXPECT_EQ(recommended_pref::kRecommendedPrefValue, actual_bool_value);

  // Test getting a default value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kDefaultPref,
                                          Value::TYPE_INTEGER, &value));
  actual_int_value = -1;
  EXPECT_TRUE(value->GetAsInteger(&actual_int_value));
  EXPECT_EQ(default_pref::kDefaultValue, actual_int_value);

  // Test getting a preference value that the |PrefValueStore|
  // does not contain.
  FundamentalValue tmp_dummy_value(true);
  Value* v_null = &tmp_dummy_value;
  ASSERT_FALSE(pref_value_store_->GetValue(prefs::kMissingPref,
                                           Value::TYPE_STRING, &v_null));
  ASSERT_TRUE(v_null == NULL);
}

TEST_F(PrefValueStoreTest, PrefChanges) {
  // Setup.
  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(_)).Times(AnyNumber());
  const char managed_platform_pref_path[] = "managed_platform_pref";
  managed_platform_pref_store_->SetString(managed_platform_pref_path,
                                                   "managed value");
  const char user_pref_path[] = "user_pref";
  user_pref_store_->SetString(user_pref_path, "user value");
  const char default_pref_path[] = "default_pref";
  default_pref_store_->SetString(default_pref_path, "default value");
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  // Check pref controlled by highest-priority store.
  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(managed_platform_pref_path));
  managed_platform_pref_store_->NotifyPrefValueChanged(
      managed_platform_pref_path);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(_)).Times(0);
  user_pref_store_->NotifyPrefValueChanged(managed_platform_pref_path);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  // Check pref controlled by user store.
  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(user_pref_path));
  managed_platform_pref_store_->NotifyPrefValueChanged(user_pref_path);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(user_pref_path));
  user_pref_store_->NotifyPrefValueChanged(user_pref_path);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(_)).Times(0);
  default_pref_store_->NotifyPrefValueChanged(user_pref_path);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  // Check pref controlled by default-pref store.
  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(default_pref_path));
  user_pref_store_->NotifyPrefValueChanged(default_pref_path);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(default_pref_path));
  default_pref_store_->NotifyPrefValueChanged(default_pref_path);
  Mock::VerifyAndClearExpectations(&pref_notifier_);
}

TEST_F(PrefValueStoreTest, OnInitializationCompleted) {
  EXPECT_CALL(pref_notifier_, OnInitializationCompleted()).Times(0);
  managed_platform_pref_store_->SetInitializationCompleted();
  device_management_pref_store_->SetInitializationCompleted();
  extension_pref_store_->SetInitializationCompleted();
  command_line_pref_store_->SetInitializationCompleted();
  recommended_pref_store_->SetInitializationCompleted();
  default_pref_store_->SetInitializationCompleted();
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  // The notification should only be triggered after the last store is done.
  EXPECT_CALL(pref_notifier_, OnInitializationCompleted()).Times(1);
  user_pref_store_->SetInitializationCompleted();
  Mock::VerifyAndClearExpectations(&pref_notifier_);
}

TEST_F(PrefValueStoreTest, PrefValueInManagedPlatformStore) {
  // Test a managed platform preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kHomePage));

  // Test a device management preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInDeviceManagementStore(
      prefs::kDefaultSearchProviderName));

  // Test an extension preference.
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kCurrentThemeID));

  // Test a command-line preference.
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kApplicationLocale));

  // Test a user preference.
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kStabilityLaunchCount));

  // Test a preference from the recommended pref store.
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kRecommendedPref));

  // Test a preference from the default pref store.
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kDefaultPref));

  // Test a preference for which the PrefValueStore does not contain a value.
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedPlatformStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueInExtensionStore) {
  // Test a managed platform preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(prefs::kHomePage));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kHomePage));

  // Test a device management preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kDefaultSearchProviderName));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kDefaultSearchProviderName));

  // Test an extension preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kCurrentThemeID));
  EXPECT_TRUE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kCurrentThemeID));

  // Test a command-line preference.
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kApplicationLocale));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kApplicationLocale));

  // Test a user preference.
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kStabilityLaunchCount));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kStabilityLaunchCount));

  // Test a preference from the recommended pref store.
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kRecommendedPref));

  // Test a preference from the default pref store.
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kDefaultPref));

  // Test a preference for which the PrefValueStore does not contain a value.
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kMissingPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueInUserStore) {
  // Test a managed platform preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(prefs::kHomePage));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(prefs::kHomePage));

  // Test a device management preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kDefaultSearchProviderName));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kDefaultSearchProviderName));

  // Test an extension preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kCurrentThemeID));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kCurrentThemeID));

  // Test a command-line preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kApplicationLocale));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kApplicationLocale));

  // Test a user preference.
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kStabilityLaunchCount));
  EXPECT_TRUE(pref_value_store_->PrefValueFromUserStore(
      prefs::kStabilityLaunchCount));

  // Test a preference from the recommended pref store.
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(
      prefs::kRecommendedPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kRecommendedPref));

  // Test a preference from the default pref store.
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(prefs::kDefaultPref));

  // Test a preference for which the PrefValueStore does not contain a value.
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(prefs::kMissingPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueFromDefaultStore) {
  // Test a managed platform preference.
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(prefs::kHomePage));

  // Test a device management preference.
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kDefaultSearchProviderName));

  // Test an extension preference.
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kCurrentThemeID));

  // Test a command-line preference.
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kApplicationLocale));

  // Test a user preference.
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kStabilityLaunchCount));

  // Test a preference from the recommended pref store.
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kRecommendedPref));

  // Test a preference from the default pref store.
  EXPECT_TRUE(
      pref_value_store_->PrefValueFromDefaultStore(prefs::kDefaultPref));

  // Test a preference for which the PrefValueStore does not contain a value.
  EXPECT_FALSE(
      pref_value_store_->PrefValueFromDefaultStore(prefs::kMissingPref));
}

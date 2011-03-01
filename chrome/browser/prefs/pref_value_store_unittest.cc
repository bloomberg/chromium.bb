// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/dummy_configuration_policy_provider.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
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

// Names of the preferences used in this test.
namespace prefs {
const char kManagedPlatformPref[] = "this.pref.managed_platform";
const char kManagedCloudPref[] = "this.pref.managed_cloud";
const char kCommandLinePref[] = "this.pref.command_line";
const char kExtensionPref[] = "this.pref.extension";
const char kUserPref[] = "this.pref.user";
const char kRecommendedPlatformPref[] = "this.pref.recommended_platform";
const char kRecommendedCloudPref[] = "this.pref.recommended_cloud";
const char kDefaultPref[] = "this.pref.default";
const char kMissingPref[] = "this.pref.does_not_exist";
}

// Potentially expected values of all preferences used in this test program.
namespace managed_platform_pref {
const char kManagedPlatformValue[] = "managed_platform:managed_platform";
}

namespace managed_cloud_pref {
const char kManagedPlatformValue[] = "managed_cloud:managed_platform";
const char kManagedCloudValue[] = "managed_cloud:managed_cloud";
}

namespace extension_pref {
const char kManagedPlatformValue[] = "extension:managed_platform";
const char kManagedCloudValue[] = "extension:managed_cloud";
const char kExtensionValue[] = "extension:extension";
}

namespace command_line_pref {
const char kManagedPlatformValue[] = "command_line:managed_platform";
const char kManagedCloudValue[] = "command_line:managed_cloud";
const char kExtensionValue[] = "command_line:extension";
const char kCommandLineValue[] = "command_line:command_line";
}

namespace user_pref {
const char kManagedPlatformValue[] = "user:managed_platform";
const char kManagedCloudValue[] = "user:managed_cloud";
const char kExtensionValue[] = "user:extension";
const char kCommandLineValue[] = "user:command_line";
const char kUserValue[] = "user:user";
}

namespace recommended_platform_pref {
const char kManagedPlatformValue[] = "recommended_platform:managed_platform";
const char kManagedCloudValue[] = "recommended_platform:managed_cloud";
const char kExtensionValue[] = "recommended_platform:extension";
const char kCommandLineValue[] = "recommended_platform:command_line";
const char kUserValue[] = "recommended_platform:user";
const char kRecommendedPlatformValue[] =
    "recommended_platform:recommended_platform";
}

namespace recommended_cloud_pref {
const char kManagedPlatformValue[] = "recommended_cloud:managed_platform";
const char kManagedCloudValue[] = "recommended_cloud:managed_cloud";
const char kExtensionValue[] = "recommended_cloud:extension";
const char kCommandLineValue[] = "recommended_cloud:command_line";
const char kUserValue[] = "recommended_cloud:user";
const char kRecommendedPlatformValue[] =
    "recommended_cloud:recommended_platform";
const char kRecommendedCloudValue[] = "recommended_cloud:recommended_cloud";
}

namespace default_pref {
const char kManagedPlatformValue[] = "default:managed_platform";
const char kManagedCloudValue[] = "default:managed_cloud";
const char kExtensionValue[] = "default:extension";
const char kCommandLineValue[] = "default:command_line";
const char kUserValue[] = "default:user";
const char kRecommendedPlatformValue[] = "default:recommended_platform";
const char kRecommendedCloudValue[] = "default:recommended_cloud";
const char kDefaultValue[] = "default:default";
}

class PrefValueStoreTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create TestingPrefStores.
    CreateManagedPlatformPrefs();
    CreateManagedCloudPrefs();
    CreateExtensionPrefs();
    CreateCommandLinePrefs();
    CreateUserPrefs();
    CreateRecommendedPlatformPrefs();
    CreateRecommendedCloudPrefs();
    CreateDefaultPrefs();

    // Create a fresh PrefValueStore.
    pref_value_store_.reset(new PrefValueStore(
        managed_platform_pref_store_,
        managed_cloud_pref_store_,
        extension_pref_store_,
        command_line_pref_store_,
        user_pref_store_,
        recommended_platform_pref_store_,
        recommended_cloud_pref_store_,
        default_pref_store_,
        &pref_notifier_));
  }

  void CreateManagedPlatformPrefs() {
    managed_platform_pref_store_ = new TestingPrefStore;
    managed_platform_pref_store_->SetString(
        prefs::kManagedPlatformPref,
        managed_platform_pref::kManagedPlatformValue);
  }

  void CreateManagedCloudPrefs() {
    managed_cloud_pref_store_ = new TestingPrefStore;
    managed_cloud_pref_store_->SetString(
        prefs::kManagedPlatformPref,
        managed_cloud_pref::kManagedPlatformValue);
    managed_cloud_pref_store_->SetString(
        prefs::kManagedCloudPref,
        managed_cloud_pref::kManagedCloudValue);
  }

  void CreateExtensionPrefs() {
    extension_pref_store_ = new TestingPrefStore;
    extension_pref_store_->SetString(
        prefs::kManagedPlatformPref,
        extension_pref::kManagedPlatformValue);
    extension_pref_store_->SetString(
        prefs::kManagedCloudPref,
        extension_pref::kManagedCloudValue);
    extension_pref_store_->SetString(
        prefs::kExtensionPref,
        extension_pref::kExtensionValue);
  }

  void CreateCommandLinePrefs() {
    command_line_pref_store_ = new TestingPrefStore;
    command_line_pref_store_->SetString(
        prefs::kManagedPlatformPref,
        command_line_pref::kManagedPlatformValue);
    command_line_pref_store_->SetString(
        prefs::kManagedCloudPref,
        command_line_pref::kManagedCloudValue);
    command_line_pref_store_->SetString(
        prefs::kExtensionPref,
        command_line_pref::kExtensionValue);
    command_line_pref_store_->SetString(
        prefs::kCommandLinePref,
        command_line_pref::kCommandLineValue);
  }

  void CreateUserPrefs() {
    user_pref_store_ = new TestingPrefStore;
    user_pref_store_->SetString(
        prefs::kManagedPlatformPref,
        user_pref::kManagedPlatformValue);
    user_pref_store_->SetString(
        prefs::kManagedCloudPref,
        user_pref::kManagedCloudValue);
    user_pref_store_->SetString(
        prefs::kCommandLinePref,
        user_pref::kCommandLineValue);
    user_pref_store_->SetString(
        prefs::kExtensionPref,
        user_pref::kExtensionValue);
    user_pref_store_->SetString(
        prefs::kUserPref,
        user_pref::kUserValue);
  }

  void CreateRecommendedPlatformPrefs() {
    recommended_platform_pref_store_ = new TestingPrefStore;
    recommended_platform_pref_store_->SetString(
        prefs::kManagedPlatformPref,
        recommended_platform_pref::kManagedPlatformValue);
    recommended_platform_pref_store_->SetString(
        prefs::kManagedCloudPref,
        recommended_platform_pref::kManagedCloudValue);
    recommended_platform_pref_store_->SetString(
        prefs::kCommandLinePref,
        recommended_platform_pref::kCommandLineValue);
    recommended_platform_pref_store_->SetString(
        prefs::kExtensionPref,
        recommended_platform_pref::kExtensionValue);
    recommended_platform_pref_store_->SetString(
        prefs::kUserPref,
        recommended_platform_pref::kUserValue);
    recommended_platform_pref_store_->SetString(
        prefs::kRecommendedPlatformPref,
        recommended_platform_pref::kRecommendedPlatformValue);
  }

  void CreateRecommendedCloudPrefs() {
    recommended_cloud_pref_store_ = new TestingPrefStore;
    recommended_cloud_pref_store_->SetString(
        prefs::kManagedPlatformPref,
        recommended_cloud_pref::kManagedPlatformValue);
    recommended_cloud_pref_store_->SetString(
        prefs::kManagedCloudPref,
        recommended_cloud_pref::kManagedCloudValue);
    recommended_cloud_pref_store_->SetString(
        prefs::kCommandLinePref,
        recommended_cloud_pref::kCommandLineValue);
    recommended_cloud_pref_store_->SetString(
        prefs::kExtensionPref,
        recommended_cloud_pref::kExtensionValue);
    recommended_cloud_pref_store_->SetString(
        prefs::kUserPref,
        recommended_cloud_pref::kUserValue);
    recommended_cloud_pref_store_->SetString(
        prefs::kRecommendedPlatformPref,
        recommended_cloud_pref::kRecommendedPlatformValue);
    recommended_cloud_pref_store_->SetString(
        prefs::kRecommendedCloudPref,
        recommended_cloud_pref::kRecommendedCloudValue);
  }

  void CreateDefaultPrefs() {
    default_pref_store_ = new TestingPrefStore;
    default_pref_store_->SetString(
        prefs::kManagedPlatformPref,
        default_pref::kManagedPlatformValue);
    default_pref_store_->SetString(
        prefs::kManagedCloudPref,
        default_pref::kManagedCloudValue);
    default_pref_store_->SetString(
        prefs::kCommandLinePref,
        default_pref::kCommandLineValue);
    default_pref_store_->SetString(
        prefs::kExtensionPref,
        default_pref::kExtensionValue);
    default_pref_store_->SetString(
        prefs::kUserPref,
        default_pref::kUserValue);
    default_pref_store_->SetString(
        prefs::kRecommendedPlatformPref,
        default_pref::kRecommendedPlatformValue);
    default_pref_store_->SetString(
        prefs::kRecommendedCloudPref,
        default_pref::kRecommendedCloudValue);
    default_pref_store_->SetString(
        prefs::kDefaultPref,
        default_pref::kDefaultValue);
  }

  MockPrefNotifier pref_notifier_;
  scoped_ptr<PrefValueStore> pref_value_store_;

  scoped_refptr<TestingPrefStore> managed_platform_pref_store_;
  scoped_refptr<TestingPrefStore> managed_cloud_pref_store_;
  scoped_refptr<TestingPrefStore> extension_pref_store_;
  scoped_refptr<TestingPrefStore> command_line_pref_store_;
  scoped_refptr<TestingPrefStore> user_pref_store_;
  scoped_refptr<TestingPrefStore> recommended_platform_pref_store_;
  scoped_refptr<TestingPrefStore> recommended_cloud_pref_store_;
  scoped_refptr<TestingPrefStore> default_pref_store_;
};

TEST_F(PrefValueStoreTest, GetValue) {
  Value* value;

  // The following tests read a value from the PrefService. The preferences are
  // set in a way such that all lower-priority stores have a value and we can
  // test whether overrides work correctly.

  // Test getting a managed platform value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kManagedPlatformPref,
                                          Value::TYPE_STRING, &value));
  std::string actual_str_value;
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(managed_platform_pref::kManagedPlatformValue, actual_str_value);

  // Test getting a managed cloud value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kManagedCloudPref,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(managed_cloud_pref::kManagedCloudValue, actual_str_value);

  // Test getting an extension value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kExtensionPref,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(extension_pref::kExtensionValue, actual_str_value);

  // Test getting a command-line value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kCommandLinePref,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(command_line_pref::kCommandLineValue, actual_str_value);

  // Test getting a user-set value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kUserPref,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(user_pref::kUserValue, actual_str_value);

  // Test getting a user set value overwriting a recommended value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kRecommendedPlatformPref,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(recommended_platform_pref::kRecommendedPlatformValue,
            actual_str_value);

  // Test getting a recommended value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kRecommendedCloudPref,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(recommended_cloud_pref::kRecommendedCloudValue, actual_str_value);

  // Test getting a default value.
  value = NULL;
  ASSERT_TRUE(pref_value_store_->GetValue(prefs::kDefaultPref,
                                          Value::TYPE_STRING, &value));
  EXPECT_TRUE(value->GetAsString(&actual_str_value));
  EXPECT_EQ(default_pref::kDefaultValue, actual_str_value);

  // Test getting a preference value that the |PrefValueStore|
  // does not contain.
  FundamentalValue tmp_dummy_value(true);
  value = &tmp_dummy_value;
  ASSERT_FALSE(pref_value_store_->GetValue(prefs::kMissingPref,
                                           Value::TYPE_STRING, &value));
  ASSERT_TRUE(value == NULL);
}

TEST_F(PrefValueStoreTest, PrefChanges) {
  // Check pref controlled by highest-priority store.
  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kManagedPlatformPref));
  managed_platform_pref_store_->NotifyPrefValueChanged(
      prefs::kManagedPlatformPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(_)).Times(0);
  managed_cloud_pref_store_->NotifyPrefValueChanged(
      prefs::kManagedPlatformPref);
  extension_pref_store_->NotifyPrefValueChanged(
      prefs::kManagedPlatformPref);
  command_line_pref_store_->NotifyPrefValueChanged(
      prefs::kManagedPlatformPref);
  user_pref_store_->NotifyPrefValueChanged(
      prefs::kManagedPlatformPref);
  recommended_platform_pref_store_->NotifyPrefValueChanged(
      prefs::kManagedPlatformPref);
  recommended_cloud_pref_store_->NotifyPrefValueChanged(
      prefs::kManagedPlatformPref);
  default_pref_store_->NotifyPrefValueChanged(
      prefs::kManagedPlatformPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  // Check pref controlled by user store.
  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kUserPref));
  managed_platform_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kUserPref));
  managed_cloud_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kUserPref));
  extension_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kUserPref));
  command_line_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kUserPref));
  user_pref_store_->NotifyPrefValueChanged(prefs::kUserPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(_)).Times(0);
  recommended_platform_pref_store_->NotifyPrefValueChanged(
      prefs::kUserPref);
  recommended_cloud_pref_store_->NotifyPrefValueChanged(
      prefs::kUserPref);
  default_pref_store_->NotifyPrefValueChanged(
      prefs::kUserPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  // Check pref controlled by default-pref store.
  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kDefaultPref));
  managed_platform_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kDefaultPref));
  managed_cloud_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kDefaultPref));
  extension_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kDefaultPref));
  command_line_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kDefaultPref));
  user_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kDefaultPref));
  recommended_platform_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kDefaultPref));
  recommended_cloud_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  EXPECT_CALL(pref_notifier_, OnPreferenceChanged(prefs::kDefaultPref));
  default_pref_store_->NotifyPrefValueChanged(prefs::kDefaultPref);
  Mock::VerifyAndClearExpectations(&pref_notifier_);
}

TEST_F(PrefValueStoreTest, OnInitializationCompleted) {
  EXPECT_CALL(pref_notifier_, OnInitializationCompleted()).Times(0);
  managed_platform_pref_store_->SetInitializationCompleted();
  managed_cloud_pref_store_->SetInitializationCompleted();
  extension_pref_store_->SetInitializationCompleted();
  command_line_pref_store_->SetInitializationCompleted();
  recommended_platform_pref_store_->SetInitializationCompleted();
  recommended_cloud_pref_store_->SetInitializationCompleted();
  default_pref_store_->SetInitializationCompleted();
  Mock::VerifyAndClearExpectations(&pref_notifier_);

  // The notification should only be triggered after the last store is done.
  EXPECT_CALL(pref_notifier_, OnInitializationCompleted()).Times(1);
  user_pref_store_->SetInitializationCompleted();
  Mock::VerifyAndClearExpectations(&pref_notifier_);
}

TEST_F(PrefValueStoreTest, PrefValueInManagedStore) {
  EXPECT_TRUE(pref_value_store_->PrefValueInManagedStore(
      prefs::kManagedPlatformPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInManagedStore(
      prefs::kManagedCloudPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kCommandLinePref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kRecommendedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kRecommendedCloudPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInManagedStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueInExtensionStore) {
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kManagedPlatformPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kManagedCloudPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kCommandLinePref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kRecommendedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kRecommendedCloudPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInExtensionStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueInUserStore) {
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kManagedPlatformPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kManagedCloudPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kExtensionPref));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kCommandLinePref));
  EXPECT_TRUE(pref_value_store_->PrefValueInUserStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(
      prefs::kRecommendedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(
      prefs::kRecommendedCloudPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueInUserStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueFromExtensionStore) {
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kManagedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kManagedCloudPref));
  EXPECT_TRUE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kCommandLinePref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kRecommendedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kRecommendedCloudPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromExtensionStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueFromUserStore) {
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kManagedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kManagedCloudPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kCommandLinePref));
  EXPECT_TRUE(pref_value_store_->PrefValueFromUserStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kRecommendedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kRecommendedCloudPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromUserStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueFromDefaultStore) {
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kManagedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kManagedCloudPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kCommandLinePref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kUserPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kRecommendedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kRecommendedCloudPref));
  EXPECT_TRUE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kDefaultPref));
  EXPECT_FALSE(pref_value_store_->PrefValueFromDefaultStore(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueUserModifiable) {
  EXPECT_FALSE(pref_value_store_->PrefValueUserModifiable(
      prefs::kManagedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueUserModifiable(
      prefs::kManagedCloudPref));
  EXPECT_FALSE(pref_value_store_->PrefValueUserModifiable(
      prefs::kExtensionPref));
  EXPECT_FALSE(pref_value_store_->PrefValueUserModifiable(
      prefs::kCommandLinePref));
  EXPECT_TRUE(pref_value_store_->PrefValueUserModifiable(
      prefs::kUserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueUserModifiable(
      prefs::kRecommendedPlatformPref));
  EXPECT_TRUE(pref_value_store_->PrefValueUserModifiable(
      prefs::kRecommendedCloudPref));
  EXPECT_TRUE(pref_value_store_->PrefValueUserModifiable(
      prefs::kDefaultPref));
  EXPECT_TRUE(pref_value_store_->PrefValueUserModifiable(
      prefs::kMissingPref));
}

TEST_F(PrefValueStoreTest, PrefValueExtensionModifiable) {
  EXPECT_FALSE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kManagedPlatformPref));
  EXPECT_FALSE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kManagedCloudPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kExtensionPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kCommandLinePref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kUserPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kRecommendedPlatformPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kRecommendedCloudPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kDefaultPref));
  EXPECT_TRUE(pref_value_store_->PrefValueExtensionModifiable(
      prefs::kMissingPref));
}

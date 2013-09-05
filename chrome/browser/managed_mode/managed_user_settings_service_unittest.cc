// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/prefs/testing_pref_store.h"
#include "chrome/browser/managed_mode/managed_user_settings_service.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kAtomicItemName[] = "X-Wombat";
const char kSettingsName[] = "TestingSetting";
const char kSettingsValue[] = "SettingsValue";
const char kSplitItemName[] = "X-SuperMoosePowers";

class ManagedUserSettingsServiceTest : public ::testing::Test {
 protected:
  ManagedUserSettingsServiceTest() {}
  virtual ~ManagedUserSettingsServiceTest() {}

  void OnNewSettingsAvailable(const base::DictionaryValue* settings) {
    if (!settings)
      settings_.reset();
    else
      settings_.reset(settings->DeepCopy());
  }

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    TestingPrefStore* pref_store = new TestingPrefStore;
    settings_service_.Init(pref_store);
    settings_service_.Subscribe(
        base::Bind(&ManagedUserSettingsServiceTest::OnNewSettingsAvailable,
                   base::Unretained(this)));
    pref_store->SetInitializationCompleted();
    ASSERT_FALSE(settings_);
    settings_service_.Activate();
    ASSERT_TRUE(settings_);
  }

  virtual void TearDown() OVERRIDE {
    settings_service_.Shutdown();
  }

  base::DictionaryValue split_items_;
  scoped_ptr<base::Value> atomic_setting_value_;
  ManagedUserSettingsService settings_service_;
  scoped_ptr<base::DictionaryValue> settings_;
};

TEST_F(ManagedUserSettingsServiceTest, SetLocalSetting) {
  const base::Value* value = NULL;
  EXPECT_FALSE(settings_->GetWithoutPathExpansion(kSettingsName, &value));

  settings_.reset();
  settings_service_.SetLocalSettingForTesting(
      kSettingsName,
      scoped_ptr<base::Value>(new base::StringValue(kSettingsValue)));
  ASSERT_TRUE(settings_);
  ASSERT_TRUE(settings_->GetWithoutPathExpansion(kSettingsName, &value));
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ(kSettingsValue, string_value);
}

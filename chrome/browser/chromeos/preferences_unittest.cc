// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include "base/prefs/pref_member.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class MyMockInputMethodManager : public input_method::MockInputMethodManager {
 public:
  MyMockInputMethodManager(StringPrefMember* previous,
                           StringPrefMember* current)
      : previous_(previous),
        current_(current) {
  }
  virtual ~MyMockInputMethodManager() {
  }

  virtual void ChangeInputMethod(const std::string& input_method_id) OVERRIDE {
    last_input_method_id_ = input_method_id;
    // Do the same thing as BrowserStateMonitor::UpdateUserPreferences.
    const std::string current_input_method_on_pref = current_->GetValue();
    if (current_input_method_on_pref == input_method_id)
      return;
    previous_->SetValue(current_input_method_on_pref);
    current_->SetValue(input_method_id);
  }

  std::string last_input_method_id_;

 private:
  StringPrefMember* previous_;
  StringPrefMember* current_;
};

}  // anonymous namespace

TEST(PreferencesTest, TestUpdatePrefOnBrowserScreenDetails) {
  chromeos::FakeUserManager* user_manager = new chromeos::FakeUserManager();
  chromeos::ScopedUserManagerEnabler user_manager_enabler(user_manager);
  const char test_user_email[] = "test_user@example.com";
  const user_manager::User* test_user = user_manager->AddUser(test_user_email);
  user_manager->LoginUser(test_user_email);

  TestingPrefServiceSyncable prefs;
  Preferences::RegisterProfilePrefs(prefs.registry());
  DownloadPrefs::RegisterProfilePrefs(prefs.registry());
  // kSelectFileLastDirectory is registered for Profile. Here we register it for
  // testing.
  prefs.registry()->RegisterStringPref(
      prefs::kSelectFileLastDirectory,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  StringPrefMember previous;
  previous.Init(prefs::kLanguagePreviousInputMethod, &prefs);
  previous.SetValue("KeyboardA");
  StringPrefMember current;
  current.Init(prefs::kLanguageCurrentInputMethod, &prefs);
  current.SetValue("KeyboardB");

  MyMockInputMethodManager mock_manager(&previous, &current);
  Preferences testee(&mock_manager);
  testee.InitUserPrefsForTesting(&prefs, test_user);
  testee.SetInputMethodListForTesting();

  // Confirm they're unchanged.
  EXPECT_EQ("KeyboardA", previous.GetValue());
  EXPECT_EQ("KeyboardB", current.GetValue());
  EXPECT_EQ("KeyboardB", mock_manager.last_input_method_id_);
}

}  // namespace chromeos

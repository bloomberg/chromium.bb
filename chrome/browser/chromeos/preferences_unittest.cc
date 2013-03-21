// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include "base/prefs/pref_member.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
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

  virtual bool SetInputMethodConfig(
      const std::string& section,
      const std::string& config_name,
      const input_method::InputMethodConfigValue& value) OVERRIDE {
    // Assume the preload engines list is "KeyboardC,KeyboardA,KeyboardB".
    // Switch to the first one, C.
    ChangeInputMethod("KeyboardC");
    return true;
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
  TestingPrefServiceSyncable prefs;
  Preferences::RegisterUserPrefs(prefs.registry());
  DownloadPrefs::RegisterUserPrefs(prefs.registry());

  StringPrefMember previous;
  previous.Init(prefs::kLanguagePreviousInputMethod, &prefs);
  previous.SetValue("KeyboardA");
  StringPrefMember current;
  current.Init(prefs::kLanguageCurrentInputMethod, &prefs);
  current.SetValue("KeyboardB");

  MyMockInputMethodManager mock_manager(&previous, &current);
  Preferences testee(&mock_manager);
  testee.InitUserPrefsForTesting(&prefs);
  testee.SetInputMethodListForTesting();

  // Confirm they're unchanged.
  EXPECT_EQ("KeyboardA", previous.GetValue());
  EXPECT_EQ("KeyboardB", current.GetValue());
  EXPECT_EQ("KeyboardB", mock_manager.last_input_method_id_);
}

}  // namespace chromeos

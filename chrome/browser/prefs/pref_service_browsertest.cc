// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/gfx/rect.h"

class PreferenceServiceTest : public InProcessBrowserTest {
 public:
  explicit PreferenceServiceTest(bool new_profile) : new_profile_(new_profile) {
  }

  virtual bool SetUpUserDataDirectory() OVERRIDE {
    FilePath user_data_directory;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);

    FilePath reference_pref_file;
    if (new_profile_) {
      reference_pref_file = ui_test_utils::GetTestFilePath(
          FilePath().AppendASCII("profiles").
                     AppendASCII("window_placement").
                     AppendASCII("Default"),
          FilePath().Append(chrome::kPreferencesFilename));
      tmp_pref_file_ = user_data_directory.AppendASCII("Default");
      CHECK(file_util::CreateDirectory(tmp_pref_file_));
      tmp_pref_file_ = tmp_pref_file_.Append(chrome::kPreferencesFilename);
    } else {
      reference_pref_file = ui_test_utils::GetTestFilePath(
          FilePath().AppendASCII("profiles").
                     AppendASCII("window_placement"),
          FilePath().Append(chrome::kLocalStateFilename));
      tmp_pref_file_ = user_data_directory.Append(chrome::kLocalStateFilename);
    }

    CHECK(file_util::PathExists(reference_pref_file));
    // Copy only the Preferences file if |new_profile_|, or Local State if not,
    // and the rest will be automatically created.
    CHECK(file_util::CopyFile(reference_pref_file, tmp_pref_file_));

#if defined(OS_WIN)
    // Make the copy writable.  On POSIX we assume the umask allows files
    // we create to be writable.
    CHECK(::SetFileAttributesW(tmp_pref_file_.value().c_str(),
        FILE_ATTRIBUTE_NORMAL));
#endif
    return true;
  }

 protected:
  FilePath tmp_pref_file_;

 private:
  bool new_profile_;
};

#if defined(OS_WIN) || defined(OS_MACOSX)
// This test verifies that the window position from the prefs file is restored
// when the app restores.  This doesn't really make sense on Linux, where
// the window manager might fight with you over positioning.  However, we
// might be able to make this work on buildbots.
// TODO(port): revisit this.

class PreservedWindowPlacementIsLoaded : public PreferenceServiceTest {
 public:
  PreservedWindowPlacementIsLoaded() : PreferenceServiceTest(true) {
  }
};

IN_PROC_BROWSER_TEST_F(PreservedWindowPlacementIsLoaded, Test) {
  // The window should open with the new reference profile, with window
  // placement values stored in the user data directory.
  JSONFileValueSerializer deserializer(tmp_pref_file_);
  scoped_ptr<Value> root(deserializer.Deserialize(NULL, NULL));

  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root.get());

  // Retrieve the screen rect for the launched window
  gfx::Rect bounds = browser()->window()->GetRestoredBounds();

  // Retrieve the expected rect values from "Preferences"
  int bottom = 0;
  std::string kBrowserWindowPlacement(prefs::kBrowserWindowPlacement);
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".bottom",
      &bottom));
  EXPECT_EQ(bottom, bounds.y() + bounds.height());

  int top = 0;
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".top",
      &top));
  EXPECT_EQ(top, bounds.y());

  int left = 0;
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".left",
      &left));
  EXPECT_EQ(left, bounds.x());

  int right = 0;
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".right",
      &right));
  EXPECT_EQ(right, bounds.x() + bounds.width());

  // Find if launched window is maximized.
  bool is_window_maximized =
      chrome::GetSavedWindowShowState(browser()) == ui::SHOW_STATE_MAXIMIZED;
  bool is_maximized = false;
  EXPECT_TRUE(root_dict->GetBoolean(kBrowserWindowPlacement + ".maximized",
      &is_maximized));
  EXPECT_EQ(is_maximized, is_window_maximized);
}
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)

class PreservedWindowPlacementIsMigrated : public PreferenceServiceTest {
 public:
  PreservedWindowPlacementIsMigrated() : PreferenceServiceTest(false) {
  }
};

IN_PROC_BROWSER_TEST_F(PreservedWindowPlacementIsMigrated, Test) {
  // The window should open with the old reference profile, with window
  // placement values stored in Local State.

  JSONFileValueSerializer deserializer(tmp_pref_file_);
  scoped_ptr<Value> root(deserializer.Deserialize(NULL, NULL));

  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  // Retrieve the screen rect for the launched window
  gfx::Rect bounds = browser()->window()->GetRestoredBounds();

  // Values from old reference profile in Local State should have been
  // correctly migrated to the user's Preferences -- if so, the window
  // should be set to values taken from the user's Local State.
  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root.get());

  // Retrieve the expected rect values from User Preferences, where they
  // should have been migrated from Local State.
  int bottom = 0;
  std::string kBrowserWindowPlacement(prefs::kBrowserWindowPlacement);
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".bottom",
      &bottom));
  EXPECT_EQ(bottom, bounds.y() + bounds.height());

  int top = 0;
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".top",
      &top));
  EXPECT_EQ(top, bounds.y());

  int left = 0;
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".left",
      &left));
  EXPECT_EQ(left, bounds.x());

  int right = 0;
  EXPECT_TRUE(root_dict->GetInteger(kBrowserWindowPlacement + ".right",
      &right));
  EXPECT_EQ(right, bounds.x() + bounds.width());

  // Find if launched window is maximized.
  bool is_window_maximized =
      chrome::GetSavedWindowShowState(browser()) == ui::SHOW_STATE_MAXIMIZED;
  bool is_maximized = false;
  EXPECT_TRUE(root_dict->GetBoolean(kBrowserWindowPlacement + ".maximized",
      &is_maximized));
  EXPECT_EQ(is_maximized, is_window_maximized);
}
#endif

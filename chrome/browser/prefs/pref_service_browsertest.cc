// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
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
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/gfx/rect.h"

typedef InProcessBrowserTest PreservedWindowPlacement;

IN_PROC_BROWSER_TEST_F(PreservedWindowPlacement, PRE_Test) {
  browser()->window()->SetBounds(gfx::Rect(20, 30, 400, 500));
}

// Fails on Chrome OS as the browser thinks it is restarting after a crash, see
// http://crbug.com/168044
#if defined(OS_CHROMEOS)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(PreservedWindowPlacement, MAYBE_Test) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  gfx::Rect bounds = browser()->window()->GetBounds();
  gfx::Rect expected_bounds(gfx::Rect(20, 30, 400, 500));
  ASSERT_EQ(expected_bounds.ToString(), bounds.ToString());
}

class PreferenceServiceTest : public InProcessBrowserTest {
 public:
  explicit PreferenceServiceTest(bool new_profile) : new_profile_(new_profile) {
  }

  virtual bool SetUpUserDataDirectory() OVERRIDE {
    base::FilePath user_data_directory;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);

    if (new_profile_) {
      original_pref_file_ = ui_test_utils::GetTestFilePath(
          base::FilePath().AppendASCII("profiles").
                     AppendASCII("window_placement").
                     AppendASCII("Default"),
          base::FilePath().Append(chrome::kPreferencesFilename));
      tmp_pref_file_ =
          user_data_directory.AppendASCII(TestingProfile::kTestUserProfileDir);
      CHECK(base::CreateDirectory(tmp_pref_file_));
      tmp_pref_file_ = tmp_pref_file_.Append(chrome::kPreferencesFilename);
    } else {
      original_pref_file_ = ui_test_utils::GetTestFilePath(
          base::FilePath().AppendASCII("profiles").
                     AppendASCII("window_placement"),
          base::FilePath().Append(chrome::kLocalStateFilename));
      tmp_pref_file_ = user_data_directory.Append(chrome::kLocalStateFilename);
    }

    CHECK(base::PathExists(original_pref_file_));
    // Copy only the Preferences file if |new_profile_|, or Local State if not,
    // and the rest will be automatically created.
    CHECK(base::CopyFile(original_pref_file_, tmp_pref_file_));

#if defined(OS_WIN)
    // Make the copy writable.  On POSIX we assume the umask allows files
    // we create to be writable.
    CHECK(::SetFileAttributesW(tmp_pref_file_.value().c_str(),
        FILE_ATTRIBUTE_NORMAL));
#endif
    return true;
  }

 protected:
  base::FilePath original_pref_file_;
  base::FilePath tmp_pref_file_;

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
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  // The window should open with the new reference profile, with window
  // placement values stored in the user data directory.
  JSONFileValueSerializer deserializer(original_pref_file_);
  scoped_ptr<base::Value> root(deserializer.Deserialize(NULL, NULL));

  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(base::Value::TYPE_DICTIONARY));

  base::DictionaryValue* root_dict =
      static_cast<base::DictionaryValue*>(root.get());

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
  bool is_window_maximized = browser()->window()->IsMaximized();
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
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  // The window should open with the old reference profile, with window
  // placement values stored in Local State.

  JSONFileValueSerializer deserializer(original_pref_file_);
  scoped_ptr<base::Value> root(deserializer.Deserialize(NULL, NULL));

  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->IsType(base::Value::TYPE_DICTIONARY));

  // Retrieve the screen rect for the launched window
  gfx::Rect bounds = browser()->window()->GetRestoredBounds();

  // Values from old reference profile in Local State should have been
  // correctly migrated to the user's Preferences -- if so, the window
  // should be set to values taken from the user's Local State.
  base::DictionaryValue* root_dict =
      static_cast<base::DictionaryValue*>(root.get());

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
  bool is_window_maximized = browser()->window()->IsMaximized();
  bool is_maximized = false;
  EXPECT_TRUE(root_dict->GetBoolean(kBrowserWindowPlacement + ".maximized",
      &is_maximized));
  EXPECT_EQ(is_maximized, is_window_maximized);
}
#endif

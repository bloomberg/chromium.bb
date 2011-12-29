// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"

class PrefsTabHelperBrowserTest : public InProcessBrowserTest {
 protected:
  virtual bool SetUpUserDataDirectory() OVERRIDE {
    FilePath test_data_directory;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
    FilePath user_data_directory;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
    FilePath default_profile = user_data_directory.AppendASCII("Default");
    if (!file_util::CreateDirectory(default_profile)) {
      LOG(ERROR) << "Can't create " << default_profile.MaybeAsASCII();
      return false;
    }
    FilePath non_global_pref_file;
    non_global_pref_file = test_data_directory
        .AppendASCII("profiles")
        .AppendASCII("webkit_global_migration")
        .AppendASCII("Default")
        .Append(chrome::kPreferencesFilename);
    if (!file_util::PathExists(non_global_pref_file)) {
      LOG(ERROR) << "Doesn't exist " << non_global_pref_file.MaybeAsASCII();
      return false;
    }
    FilePath default_pref_file =
        default_profile.Append(chrome::kPreferencesFilename);
    if (!file_util::CopyFile(non_global_pref_file, default_pref_file)) {
      LOG(ERROR) << "Copy error from " << non_global_pref_file.MaybeAsASCII()
                 << " to " << default_pref_file.MaybeAsASCII();
      return false;
    }

#if defined(OS_WIN)
    // Make the copy writable.  On POSIX we assume the umask allows files
    // we create to be writable.
    if (!::SetFileAttributesW(default_pref_file.value().c_str(),
                              FILE_ATTRIBUTE_NORMAL)) return false;
#endif
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, NonGlobalPrefsAreMigrated) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kDefaultCharset));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitDefaultFontSize));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitDefaultFixedFontSize));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitMinimumFontSize));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitMinimumLogicalFontSize));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitCursiveFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitFantasyFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitFixedFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitSansSerifFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitSerifFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitStandardFontFamily));

  EXPECT_EQ("ISO-8859-1", prefs->GetString(prefs::kGlobalDefaultCharset));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitGlobalDefaultFontSize));
  EXPECT_EQ(42,
            prefs->GetInteger(prefs::kWebKitGlobalDefaultFixedFontSize));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitGlobalMinimumFontSize));
  EXPECT_EQ(42,
            prefs->GetInteger(prefs::kWebKitGlobalMinimumLogicalFontSize));
  EXPECT_EQ("CursiveFontFamily",
            prefs->GetString(prefs::kWebKitGlobalCursiveFontFamily));
  EXPECT_EQ("FantasyFontFamily",
            prefs->GetString(prefs::kWebKitGlobalFantasyFontFamily));
  EXPECT_EQ("FixedFontFamily",
            prefs->GetString(prefs::kWebKitGlobalFixedFontFamily));
  EXPECT_EQ("SansSerifFontFamily",
            prefs->GetString(prefs::kWebKitGlobalSansSerifFontFamily));
  EXPECT_EQ("SerifFontFamily",
            prefs->GetString(prefs::kWebKitGlobalSerifFontFamily));
  EXPECT_EQ("StandardFontFamily",
            prefs->GetString(prefs::kWebKitGlobalStandardFontFamily));
}

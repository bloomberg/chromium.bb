// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  virtual FilePath GetPreferencesFilePath() {
    FilePath test_data_directory;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
    return test_data_directory
        .AppendASCII("profiles")
        .AppendASCII("webkit_global_migration")
        .AppendASCII("Default")
        .Append(chrome::kPreferencesFilename);
  }

  virtual bool SetUpUserDataDirectory() OVERRIDE {
    FilePath user_data_directory;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
    FilePath default_profile = user_data_directory.AppendASCII("Default");
    if (!file_util::CreateDirectory(default_profile)) {
      LOG(ERROR) << "Can't create " << default_profile.MaybeAsASCII();
      return false;
    }
    FilePath non_global_pref_file = GetPreferencesFilePath();
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

// This tests migration like:
// webkit.webprefs.default_charset -> webkit.webprefs.global.default_charset
// This was needed for per-tab prefs, which have since been removed. So
// eventually this migration will be replaced with the reverse migration.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, NonGlobalPrefsAreMigrated) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kDefaultCharset));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitDefaultFontSize));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitDefaultFixedFontSize));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitMinimumFontSize));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitMinimumLogicalFontSize));

  EXPECT_EQ("ISO-8859-1", prefs->GetString(prefs::kGlobalDefaultCharset));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitGlobalDefaultFontSize));
  EXPECT_EQ(42,
            prefs->GetInteger(prefs::kWebKitGlobalDefaultFixedFontSize));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitGlobalMinimumFontSize));
  EXPECT_EQ(42,
            prefs->GetInteger(prefs::kWebKitGlobalMinimumLogicalFontSize));
};

// This tests migration like:
// webkit.webprefs.standard_font_family -> webkit.webprefs.fonts.standard.Zyyy
// This migration moves the formerly "non-per-script" font prefs into the
// per-script font maps, as the entry for "Common" script (Zyyy is the ISO 15924
// script code for the Common script).
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, PrefsAreMigratedToFontMap) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitOldCursiveFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitOldFantasyFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitOldFixedFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitOldSansSerifFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitOldSerifFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitOldStandardFontFamily));
  EXPECT_EQ("CursiveFontFamily",
            prefs->GetString(prefs::kWebKitCursiveFontFamily));
  EXPECT_EQ("FantasyFontFamily",
            prefs->GetString(prefs::kWebKitFantasyFontFamily));
  EXPECT_EQ("FixedFontFamily",
            prefs->GetString(prefs::kWebKitFixedFontFamily));
  EXPECT_EQ("SansSerifFontFamily",
            prefs->GetString(prefs::kWebKitSansSerifFontFamily));
  EXPECT_EQ("SerifFontFamily",
            prefs->GetString(prefs::kWebKitSerifFontFamily));
  EXPECT_EQ("StandardFontFamily",
            prefs->GetString(prefs::kWebKitStandardFontFamily));
};

class PrefsTabHelperBrowserTest2 : public PrefsTabHelperBrowserTest {
 protected:
  virtual FilePath GetPreferencesFilePath() OVERRIDE {
    FilePath test_data_directory;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
    return test_data_directory
        .AppendASCII("profiles")
        .AppendASCII("webkit_global_reverse_migration")
        .AppendASCII("Default")
        .Append(chrome::kPreferencesFilename);
  }
};

// This tests migration like:
// webkit.webprefs.global.standard_font_family ->
// webkit.webprefs.fonts.standard.Zyyy
// This undoes the migration to "global" names (originally done for the per-tab
// pref mechanism, which has since been removed). In addition, it moves the
// formerly "non-per-script" font prefs into the per-script font maps, as
// described in the comment for PrefsAreMigratedToFontMap.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest2, GlobalPrefsAreMigrated) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitGlobalCursiveFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitGlobalFantasyFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitGlobalFixedFontFamily));
  EXPECT_EQ(NULL,
            prefs->FindPreference(prefs::kWebKitGlobalSansSerifFontFamily));
  EXPECT_EQ(NULL, prefs->FindPreference(prefs::kWebKitGlobalSerifFontFamily));
  EXPECT_EQ(NULL,
            prefs->FindPreference(prefs::kWebKitGlobalStandardFontFamily));

  EXPECT_EQ("CursiveFontFamily",
            prefs->GetString(prefs::kWebKitCursiveFontFamily));
  EXPECT_EQ("FantasyFontFamily",
            prefs->GetString(prefs::kWebKitFantasyFontFamily));
  EXPECT_EQ("FixedFontFamily",
            prefs->GetString(prefs::kWebKitFixedFontFamily));
  EXPECT_EQ("SansSerifFontFamily",
            prefs->GetString(prefs::kWebKitSansSerifFontFamily));
  EXPECT_EQ("SerifFontFamily",
            prefs->GetString(prefs::kWebKitSerifFontFamily));
  EXPECT_EQ("StandardFontFamily",
            prefs->GetString(prefs::kWebKitStandardFontFamily));
};

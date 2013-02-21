// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"

class PrefsTabHelperBrowserTest : public InProcessBrowserTest {
 protected:
  virtual base::FilePath GetPreferencesFilePath() {
    base::FilePath test_data_directory;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
    return test_data_directory
        .AppendASCII("profiles")
        .AppendASCII("webkit_global_migration")
        .AppendASCII("Default")
        .Append(chrome::kPreferencesFilename);
  }

  virtual bool SetUpUserDataDirectory() OVERRIDE {
    base::FilePath user_data_directory;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
    base::FilePath default_profile =
        user_data_directory.AppendASCII(TestingProfile::kTestUserProfileDir);
    if (!file_util::CreateDirectory(default_profile)) {
      LOG(ERROR) << "Can't create " << default_profile.MaybeAsASCII();
      return false;
    }
    base::FilePath non_global_pref_file = GetPreferencesFilePath();
    if (!file_util::PathExists(non_global_pref_file)) {
      LOG(ERROR) << "Doesn't exist " << non_global_pref_file.MaybeAsASCII();
      return false;
    }
    base::FilePath default_pref_file =
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
// webkit.webprefs.standard_font_family -> webkit.webprefs.fonts.standard.Zyyy
// This migration moves the formerly "non-per-script" font prefs into the
// per-script font maps, as the entry for "Common" script (Zyyy is the ISO 15924
// script code for the Common script).
//
// In addition, it tests that the former migration of
// webkit.webprefs.blahblah -> webkit.webprefs.global.blahblah
// no longer occurs.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest, PrefsAreMigratedToFontMap) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  EXPECT_TRUE(prefs->FindPreference(
      prefs::kGlobalDefaultCharset)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalDefaultFontSize)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalDefaultFixedFontSize)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalMinimumFontSize)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalMinimumLogicalFontSize)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitOldCursiveFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitOldFantasyFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitOldFixedFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitOldSansSerifFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitOldSerifFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitOldStandardFontFamily)->IsDefaultValue());

  EXPECT_EQ("ISO-8859-1", prefs->GetString(prefs::kDefaultCharset));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitDefaultFontSize));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitMinimumFontSize));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize));
  EXPECT_EQ("CursiveFontFamily",
            prefs->GetString(prefs::kWebKitCursiveFontFamily));
  EXPECT_EQ("FantasyFontFamily",
            prefs->GetString(prefs::kWebKitFantasyFontFamily));
  // PictographFontFamily was added after the migration, so it never exists
  // in the old format (and consequently isn't in the test Preferences file).
  // So it doesn't need to be tested here.
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
  virtual base::FilePath GetPreferencesFilePath() OVERRIDE {
    base::FilePath test_data_directory;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
    return test_data_directory
        .AppendASCII("profiles")
        .AppendASCII("webkit_global_reverse_migration")
        .AppendASCII("Default")
        .Append(chrome::kPreferencesFilename);
  }
};

// This tests migration like:
// webkit.webprefs.global.blahblah -> webkit.webprefs.blahblah
// This undoes the migration to "global" names (originally done for the per-tab
// pref mechanism, which has since been removed).
//
// In addition it tests the migration for font families:
// webkit.webprefs.global.standard_font_family ->
// webkit.webprefs.fonts.standard.Zyyy
// This moves the formerly "non-per-script" font prefs into the per-script font
// maps, as described in the comment for PrefsAreMigratedToFontMap.
IN_PROC_BROWSER_TEST_F(PrefsTabHelperBrowserTest2, GlobalPrefsAreMigrated) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  EXPECT_TRUE(prefs->FindPreference(
      prefs::kGlobalDefaultCharset)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalDefaultFontSize)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalDefaultFixedFontSize)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalMinimumFontSize)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalMinimumLogicalFontSize)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalCursiveFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalFantasyFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalFixedFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalSansSerifFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalSerifFontFamily)->IsDefaultValue());
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kWebKitGlobalStandardFontFamily)->IsDefaultValue());

  EXPECT_EQ("ISO-8859-1", prefs->GetString(prefs::kDefaultCharset));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitDefaultFontSize));
  EXPECT_EQ(42,
            prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize));
  EXPECT_EQ(42, prefs->GetInteger(prefs::kWebKitMinimumFontSize));
  EXPECT_EQ(42,
            prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize));
  EXPECT_EQ("CursiveFontFamily",
            prefs->GetString(prefs::kWebKitCursiveFontFamily));
  EXPECT_EQ("FantasyFontFamily",
            prefs->GetString(prefs::kWebKitFantasyFontFamily));
  // PictographFontFamily was added after the migration, so it never exists
  // in the old format (and consequently isn't in the test Preferences file).
  // So it doesn't need to be tested here.
  EXPECT_EQ("FixedFontFamily",
            prefs->GetString(prefs::kWebKitFixedFontFamily));
  EXPECT_EQ("SansSerifFontFamily",
            prefs->GetString(prefs::kWebKitSansSerifFontFamily));
  EXPECT_EQ("SerifFontFamily",
            prefs->GetString(prefs::kWebKitSerifFontFamily));
  EXPECT_EQ("StandardFontFamily",
            prefs->GetString(prefs::kWebKitStandardFontFamily));
};
